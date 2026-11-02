cmake_minimum_required(VERSION 3.16)
get_filename_component(PASSPORT_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(MUSE_SDK "${PASSPORT_ROOT}/build/muse-source")
if(NOT EXISTS "${MUSE_SDK}/main/app.c")
    message(FATAL_ERROR "Run tools/prepare_passport_muse.py --sdk <pinned checkout> first")
endif()
set(EXTRA_COMPONENT_DIRS "${MUSE_SDK}/main" "${MUSE_SDK}/components" "${CMAKE_CURRENT_LIST_DIR}/components")
# The pinned SDK's sdkconfig.defaults starts with its ESP32-C5 board, and
# ESP-IDF guesses the target from the first defaults file that names one. Pin
# this project's target before that guess; a -DIDF_TARGET override still wins.
if(NOT IDF_TARGET)
    set(IDF_TARGET "esp32c3")
endif()
set(SDKCONFIG "${CMAKE_BINARY_DIR}/sdkconfig")
set(SDKCONFIG_DEFAULTS "${MUSE_SDK}/sdkconfig.defaults;${MUSE_SDK}/devices/sdkconfig.muse;${MUSE_SDK}/devices/sdkconfig.muse-ai-passport;${CMAKE_CURRENT_LIST_DIR}/sdkconfig.defaults")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(FoloToy-AI-Passport)
idf_component_get_property(muse_lib muse COMPONENT_LIB)
# Passport's existing LVGL port owns the display task, not the SDK's UI.
target_compile_definitions(${muse_lib} PRIVATE PASSPORT_MUSE_INTEGRATION=1)
# Half-height LVGL draw buffer: frees 9.6 KB of internal DMA RAM for the Muse
# control session on this PSRAM-less board.
idf_component_get_property(bsp_lib bsp COMPONENT_LIB)
target_compile_definitions(${bsp_lib} PRIVATE BSP_LVGL_DRAW_BUFFER_LINES=20)
