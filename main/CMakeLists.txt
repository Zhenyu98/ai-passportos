include("${CMAKE_CURRENT_LIST_DIR}/../tools/cmake/passport_assets.cmake")
idf_component_register(
    SRCS "main.c" "passport_app.c" "passport_screen.c" "passport_ui.c" "passport_model.c" "passport_animation.c" "passport_scene.c"
         "passport_game.c" "passport_profile.c" "passport_store.c" "passport_nvs.c"
         "passport_protocol.c" "passport_usb.c" "passport_qr.c"
         "passport_json.c" "passport_settings.c"
         "passport_config.c" "passport_config_auth.c" "passport_avatar_flash.c" "passport_agent_model.c" "passport_agent.c"
         "passport_speech.c" "passport_speech_http.c"
         "${PASSPORT_ASSETS}/fonts/passport_font_16.c"
         "${PASSPORT_ASSETS}/fonts/passport_font_20.c"
         "${PASSPORT_BOLD_FONT}"
         "${PASSPORT_ASSETS}/images/passport_avatar.c"
         "${PASSPORT_MOTION}"
         "${PASSPORT_AGENT_AVATAR}"
         "${PASSPORT_ASSETS}/images/passport_gallery.c"
         "${PASSPORT_ASSETS}/passport/passport_glyphs.c"
    INCLUDE_DIRS "." "${PASSPORT_ASSETS}/passport"
    REQUIRES bsp json esp_timer nvs_flash esp_driver_usb_serial_jtag
             esp_wifi esp_http_server esp_http_client mbedtls esp_netif esp_event esp_partition)
idf_component_get_property(PASSPORT_LVGL_DIR lvgl__lvgl COMPONENT_DIR)
target_include_directories(${COMPONENT_LIB} PRIVATE "${PASSPORT_LVGL_DIR}/src/libs/qrcode")
