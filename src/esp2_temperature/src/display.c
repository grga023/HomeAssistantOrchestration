/* Minimal SPI LCD driver for the ESP-WROVER-KIT V4.1 panel. See display.h.
 *
 * The ILI9341/ST7789 init tables and the command/data + D/C pre-callback
 * mechanism are adapted from the public-domain ESP-IDF spi_master/lcd example
 * (Espressif, CC0). Text rendering and the public API are ours.
 */
#include "display.h"
#include "font8x8.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_heap_caps.h"
#include "esp_log.h"

#define LCD_HOST      SPI2_HOST
#define PIN_NUM_MISO  25
#define PIN_NUM_MOSI  23
#define PIN_NUM_CLK   19
#define PIN_NUM_CS    22
#define PIN_NUM_DC    21
#define PIN_NUM_RST   18
#define PIN_NUM_BCKL  5
#define LCD_BK_LIGHT_ON_LEVEL 0   /* backlight is active-low on the WROVER-KIT */

/* Reusable DMA scratch buffer, in pixels. Bounds a single SPI transfer and the
 * largest glyph cell (scale<=4 -> 32*32 = 1024 px). */
#define SCRATCH_PX 1024

static const char *TAG = "display";
static spi_device_handle_t s_spi;
static uint16_t *s_scratch;          /* DMA-capable, SCRATCH_PX entries */
static SemaphoreHandle_t s_lock;

typedef struct {
    uint8_t cmd;
    uint8_t data[16];
    uint8_t databytes;   /* bit7 = delay after; 0x1F mask = count; 0xFF = end */
} lcd_init_cmd_t;

typedef enum { LCD_TYPE_ILI = 1, LCD_TYPE_ST } lcd_type_t;

DRAM_ATTR static const lcd_init_cmd_t st_init_cmds[] = {
    {0x36, {(1 << 5) | (1 << 6)}, 1},
    {0x3A, {0x55}, 1},
    {0xB2, {0x0c, 0x0c, 0x00, 0x33, 0x33}, 5},
    {0xB7, {0x45}, 1},
    {0xBB, {0x2B}, 1},
    {0xC0, {0x2C}, 1},
    {0xC2, {0x01, 0xff}, 2},
    {0xC3, {0x11}, 1},
    {0xC4, {0x20}, 1},
    {0xC6, {0x0f}, 1},
    {0xD0, {0xA4, 0xA1}, 2},
    {0xE0, {0xD0, 0x00, 0x05, 0x0E, 0x15, 0x0D, 0x37, 0x43, 0x47, 0x09, 0x15, 0x12, 0x16, 0x19}, 14},
    {0xE1, {0xD0, 0x00, 0x05, 0x0D, 0x0C, 0x06, 0x2D, 0x44, 0x40, 0x0E, 0x1C, 0x18, 0x16, 0x19}, 14},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

DRAM_ATTR static const lcd_init_cmd_t ili_init_cmds[] = {
    {0xCF, {0x00, 0x83, 0X30}, 3},
    {0xED, {0x64, 0x03, 0X12, 0X81}, 4},
    {0xE8, {0x85, 0x01, 0x79}, 3},
    {0xCB, {0x39, 0x2C, 0x00, 0x34, 0x02}, 5},
    {0xF7, {0x20}, 1},
    {0xEA, {0x00, 0x00}, 2},
    {0xC0, {0x26}, 1},
    {0xC1, {0x11}, 1},
    {0xC5, {0x35, 0x3E}, 2},
    {0xC7, {0xBE}, 1},
    {0x36, {0x28}, 1},
    {0x3A, {0x55}, 1},
    {0xB1, {0x00, 0x1B}, 2},
    {0xF2, {0x08}, 1},
    {0x26, {0x01}, 1},
    {0xE0, {0x1F, 0x1A, 0x18, 0x0A, 0x0F, 0x06, 0x45, 0X87, 0x32, 0x0A, 0x07, 0x02, 0x07, 0x05, 0x00}, 15},
    {0XE1, {0x00, 0x25, 0x27, 0x05, 0x10, 0x09, 0x3A, 0x78, 0x4D, 0x05, 0x18, 0x0D, 0x38, 0x3A, 0x1F}, 15},
    {0x2A, {0x00, 0x00, 0x00, 0xEF}, 4},
    {0x2B, {0x00, 0x00, 0x01, 0x3f}, 4},
    {0x2C, {0}, 0},
    {0xB7, {0x07}, 1},
    {0xB6, {0x0A, 0x82, 0x27, 0x00}, 4},
    {0x11, {0}, 0x80},
    {0x29, {0}, 0x80},
    {0, {0}, 0xff},
};

/* pre-transmit callback: drive the D/C line from the transaction's user field */
static void IRAM_ATTR pre_transfer_cb(spi_transaction_t *t)
{
    gpio_set_level(PIN_NUM_DC, (int)(intptr_t)t->user);
}

static void lcd_cmd(uint8_t cmd)
{
    spi_transaction_t t = {0};
    t.length = 8;
    t.tx_buffer = &cmd;
    t.user = (void *)0;   /* command */
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static void lcd_data(const uint8_t *data, int len)
{
    if (len <= 0) return;
    spi_transaction_t t = {0};
    t.length = (size_t)len * 8;
    t.tx_buffer = data;
    t.user = (void *)1;   /* data */
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
}

static uint32_t lcd_get_id(void)
{
    spi_device_acquire_bus(s_spi, portMAX_DELAY);
    lcd_cmd(0x04);
    spi_transaction_t t = {0};
    t.length = 8 * 3;
    t.flags = SPI_TRANS_USE_RXDATA;
    t.user = (void *)1;
    ESP_ERROR_CHECK(spi_device_polling_transmit(s_spi, &t));
    spi_device_release_bus(s_spi);
    return *(uint32_t *)t.rx_data;
}

static void lcd_run_init_table(const lcd_init_cmd_t *cmds)
{
    for (int i = 0; cmds[i].databytes != 0xff; i++) {
        lcd_cmd(cmds[i].cmd);
        lcd_data(cmds[i].data, cmds[i].databytes & 0x1F);
        if (cmds[i].databytes & 0x80) {
            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }
}

static void lcd_panel_init(void)
{
    gpio_config_t io = {
        .pin_bit_mask = (1ULL << PIN_NUM_DC) | (1ULL << PIN_NUM_RST) | (1ULL << PIN_NUM_BCKL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = true,
    };
    gpio_config(&io);

    gpio_set_level(PIN_NUM_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(PIN_NUM_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    uint32_t id = lcd_get_id();
    lcd_type_t type = (id == 0) ? LCD_TYPE_ILI : LCD_TYPE_ST;
    ESP_LOGI(TAG, "LCD ID=0x%08lx -> %s", (unsigned long)id,
             type == LCD_TYPE_ILI ? "ILI9341" : "ST7789V");

    lcd_run_init_table(type == LCD_TYPE_ILI ? ili_init_cmds : st_init_cmds);
    gpio_set_level(PIN_NUM_BCKL, LCD_BK_LIGHT_ON_LEVEL);
}

/* Address window: [x0,x1] x [y0,y1] inclusive, then start RAM write (0x2C). */
static void lcd_set_window(int x0, int y0, int x1, int y1)
{
    uint8_t d[4];
    lcd_cmd(0x2A);
    d[0] = (uint8_t)(x0 >> 8); d[1] = (uint8_t)x0;
    d[2] = (uint8_t)(x1 >> 8); d[3] = (uint8_t)x1;
    lcd_data(d, 4);
    lcd_cmd(0x2B);
    d[0] = (uint8_t)(y0 >> 8); d[1] = (uint8_t)y0;
    d[2] = (uint8_t)(y1 >> 8); d[3] = (uint8_t)y1;
    lcd_data(d, 4);
    lcd_cmd(0x2C);
}

/* Push `count` pixels (already byte-swapped to big-endian) from s_scratch. */
static void lcd_push_scratch(int count)
{
    lcd_data((const uint8_t *)s_scratch, count * 2);
}

static inline uint16_t swap16(uint16_t c) { return (uint16_t)((c >> 8) | (c << 8)); }

/* Clip a rectangle to the panel; returns false if fully off-screen. */
static bool clip_rect(int *x, int *y, int *w, int *h)
{
    if (*x < 0) { *w += *x; *x = 0; }
    if (*y < 0) { *h += *y; *y = 0; }
    if (*x >= LCD_WIDTH || *y >= LCD_HEIGHT) return false;
    if (*x + *w > LCD_WIDTH)  *w = LCD_WIDTH  - *x;
    if (*y + *h > LCD_HEIGHT) *h = LCD_HEIGHT - *y;
    return (*w > 0 && *h > 0);
}

static void fill_rect_nolock(int x, int y, int w, int h, uint16_t color)
{
    if (!clip_rect(&x, &y, &w, &h)) return;

    uint16_t be = swap16(color);
    int total = w * h;
    int fill = total < SCRATCH_PX ? total : SCRATCH_PX;
    for (int i = 0; i < fill; i++) s_scratch[i] = be;

    lcd_set_window(x, y, x + w - 1, y + h - 1);
    while (total > 0) {
        int n = total < SCRATCH_PX ? total : SCRATCH_PX;
        lcd_push_scratch(n);
        total -= n;
    }
}

/* Render one glyph into s_scratch and blit it. cell = 8*scale square. */
static void draw_char_nolock(int x, int y, char c, uint16_t fg, uint16_t bg, int scale)
{
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    const uint8_t *glyph = font8x8_basic[(unsigned char)c & 0x7F];
    int cell = 8 * scale;
    uint16_t fbe = swap16(fg), bbe = swap16(bg);

    for (int row = 0; row < 8; row++) {
        uint8_t bits = glyph[row];
        for (int col = 0; col < 8; col++) {
            uint16_t px = (bits & (1 << col)) ? fbe : bbe;
            int bx = col * scale, by = row * scale;
            for (int sy = 0; sy < scale; sy++) {
                uint16_t *dst = &s_scratch[(by + sy) * cell + bx];
                for (int sx = 0; sx < scale; sx++) dst[sx] = px;
            }
        }
    }

    if (x < 0 || y < 0 || x + cell > LCD_WIDTH || y + cell > LCD_HEIGHT) return;
    lcd_set_window(x, y, x + cell - 1, y + cell - 1);
    lcd_push_scratch(cell * cell);
}

void display_fill_screen(uint16_t color)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    fill_rect_nolock(0, 0, LCD_WIDTH, LCD_HEIGHT, color);
    xSemaphoreGive(s_lock);
}

void display_fill_rect(int x, int y, int w, int h, uint16_t color)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    fill_rect_nolock(x, y, w, h, color);
    xSemaphoreGive(s_lock);
}

void display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale)
{
    if (!s) return;
    if (scale < 1) scale = 1;
    if (scale > 4) scale = 4;
    int cell = 8 * scale;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    for (int cx = x; *s; s++, cx += cell) {
        draw_char_nolock(cx, y, *s, fg, bg, scale);
    }
    xSemaphoreGive(s_lock);
}

void display_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_scratch = heap_caps_malloc(SCRATCH_PX * sizeof(uint16_t), MALLOC_CAP_DMA);
    assert(s_scratch != NULL);

    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_NUM_MISO,
        .mosi_io_num = PIN_NUM_MOSI,
        .sclk_io_num = PIN_NUM_CLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SCRATCH_PX * 2 + 16,
    };
    spi_device_interface_config_t devcfg = {
        /* ESP32 SPI master caps at 26.67 MHz for GPIO-matrix-routed pins. */
        .clock_speed_hz = 26 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = PIN_NUM_CS,
        .queue_size = 7,
        .pre_cb = pre_transfer_cb,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(LCD_HOST, &buscfg, SPI_DMA_CH_AUTO));
    ESP_ERROR_CHECK(spi_bus_add_device(LCD_HOST, &devcfg, &s_spi));

    lcd_panel_init();
    display_fill_screen(C_BLACK);
}
