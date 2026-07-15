# Main component. The active board is selected by platformio.ini via
#   board_build.cmake_extra_args = -DSMARTHOME_BOARD=<board>
# Only that board's sources + include dir are compiled, so there is exactly
# one app_main() in the build.
if(NOT DEFINED SMARTHOME_BOARD)
    set(SMARTHOME_BOARD esp1_lights)
endif()

file(GLOB_RECURSE APP_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/${SMARTHOME_BOARD}/src/*.c)

idf_component_register(
    SRCS ${APP_SOURCES}
    INCLUDE_DIRS "${SMARTHOME_BOARD}/include"
    REQUIRES common driver json esp_timer
)
