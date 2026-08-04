/* Minimal SPI LCD driver for the ESP-WROVER-KIT V4.1 on-board 320x240 panel.
 *
 * Auto-detects the ILI9341 or ST7789V controller (as the ESP-IDF WROVER-KIT
 * example does) and renders solid rectangles + 8x8 bitmap text. Landscape
 * orientation (320 wide x 240 tall). RGB565 colors.
 *
 * Wiring is fixed to the WROVER-KIT V4.1: SCLK=19 MOSI=23 MISO=25 CS=22
 * DC=21 RST=18 BCKL=5 on SPI2_HOST.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define LCD_WIDTH  320
#define LCD_HEIGHT 240

/* RGB565 helpers / common colors. */
#define RGB565(r, g, b) ((uint16_t)((((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3)))
#define C_BLACK   0x0000
#define C_WHITE   0xFFFF
#define C_RED     0xF800
#define C_GREEN   0x07E0
#define C_BLUE    0x001F
#define C_CYAN    0x07FF
#define C_YELLOW  0xFFE0
#define C_ORANGE  0xFD20
#define C_NAVY    0x0010

/* Bring up SPI + the panel and clear it to black. Call once, before drawing. */
void display_init(void);

/* Fill the whole screen with one color. */
void display_fill_screen(uint16_t color);

/* Fill a rectangle (clipped to the panel). */
void display_fill_rect(int x, int y, int w, int h, uint16_t color);

/* Draw a NUL-terminated ASCII string with the 8x8 font, integer-scaled.
 * Each glyph occupies 8*scale x 8*scale pixels; bg fills the glyph cell. */
void display_text(int x, int y, const char *s, uint16_t fg, uint16_t bg, int scale);

#ifdef __cplusplus
}
#endif
