get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
include("${ROOT}/tools/cmake/passport_assets.cmake")
set(app_srcs passport_app passport_ui passport_model passport_animation passport_scene
    passport_game passport_profile passport_store passport_nvs passport_protocol
    passport_usb passport_qr passport_json passport_settings passport_config passport_config_auth
    passport_avatar_flash passport_agent_model passport_agent passport_muse passport_muse_tools passport_speech passport_speech_http)
set(srcs)
foreach(src IN LISTS app_srcs)
    list(APPEND srcs "${ROOT}/main/${src}.c")
endforeach()
idf_component_register(SRCS ${srcs}
    "${PASSPORT_ASSETS}/fonts/passport_font_16.c" "${PASSPORT_ASSETS}/fonts/passport_font_20.c"
    "${PASSPORT_BOLD_FONT}"
    "${PASSPORT_ASSETS}/images/passport_avatar.c" "${PASSPORT_MOTION}" "${PASSPORT_AGENT_AVATAR}"
    "${PASSPORT_ASSETS}/images/passport_gallery.c" "${PASSPORT_ASSETS}/passport/passport_glyphs.c"
    INCLUDE_DIRS "${ROOT}/main" "${PASSPORT_ASSETS}/passport"
    REQUIRES bsp muse espressif__cjson esp_timer nvs_flash esp_driver_usb_serial_jtag
        esp_wifi esp_http_server esp_http_client mbedtls esp_netif esp_event esp_partition)
target_compile_definitions(${COMPONENT_LIB} PRIVATE PASSPORT_MUSE_INTEGRATION=1)
idf_component_get_property(LVGL_DIR lvgl__lvgl COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${LVGL_DIR}/src/libs/qrcode")
# Passport's glue implements the SDK's application entry point and uses headers
# from the generated SDK's main component (console command hook). The SDK main
# component requires this one, so include the directory instead of a REQUIRE
# cycle.
target_include_directories(${COMPONENT_LIB} PRIVATE "${ROOT}/build/muse-source/main")
