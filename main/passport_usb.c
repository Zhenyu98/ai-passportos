#include "passport_usb.h"
#include "passport_speech.h"
#include "driver/usb_serial_jtag.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>

static TaskHandle_t s_task;
static SemaphoreHandle_t s_exited;
static passport_service_fn s_service;
static void *s_user;
static bool s_driver_owned;
static atomic_bool s_stop_requested;
static passport_line_reader_t s_reader;
static char s_response[PASSPORT_LINE_MAX];
__attribute__((weak)) bool passport_usb_external_line(const char *line,size_t length) {
    (void)line;(void)length;return false;
}

static void send_reply(passport_reply_t *reply) {
    size_t size=0;
    if (passport_protocol_reply(reply,s_response,sizeof s_response,&size)!=PASSPORT_OK) {
        reply->status=PASSPORT_ERR_CAPACITY;
        reply->glyph_count=0;
        if (passport_protocol_reply(reply,s_response,sizeof s_response,&size)!=PASSPORT_OK) return;
    }
    /* Leave the boot console's nonblocking VFS unchanged. Drain the response
     * while holding its FILE lock so normal logs cannot split a JSON line. */
    flockfile(stdout);
    int sent=usb_serial_jtag_write_bytes(s_response,size,pdMS_TO_TICKS(100));
    if (sent==(int)size) (void)usb_serial_jtag_wait_tx_done(pdMS_TO_TICKS(100));
    funlockfile(stdout);
}

static void usb_worker(void *arg) {
    (void)arg;
    uint8_t chunk[128];
    while (!atomic_load(&s_stop_requested)) {
        int count=usb_serial_jtag_read_bytes(chunk,sizeof chunk,pdMS_TO_TICKS(10));
        for (int i=0;i<count && !atomic_load(&s_stop_requested);++i) {
            const char *line;
            size_t length;
            passport_line_event_t event=passport_line_feed(&s_reader,chunk[i],&line,&length);
            if (event==PASSPORT_LINE_NONE) continue;
            passport_request_t request;
            passport_reply_t reply={0};
            if (event==PASSPORT_LINE_OVERFLOW) {
                reply.status=PASSPORT_ERR_CAPACITY;
            } else {
                if(passport_speech_configuration_line(line,length) || passport_usb_external_line(line,length)) continue;
                reply.status=passport_protocol_parse(line,length,&request);
                if (reply.status==PASSPORT_OK) {
                    reply.status=s_service(&request,&reply,s_user);
                    reply.id=request.id;
                    reply.op=request.op;
                }
            }
            send_reply(&reply);
        }
    }
    /* No shared state access after this acknowledgement. The owner deletes us. */
    xSemaphoreGive(s_exited);
    vTaskSuspend(NULL);
}

static esp_err_t cleanup(void) {
    if (s_exited) { vSemaphoreDelete(s_exited); s_exited=NULL; }
    if (s_driver_owned) {
        esp_err_t e=usb_serial_jtag_driver_uninstall();
        if (e!=ESP_OK) return e;
        s_driver_owned=false;
    }
    return ESP_OK;
}

esp_err_t passport_usb_start(passport_service_fn service, void *user) {
    if (!service) return ESP_ERR_INVALID_ARG;
    if (s_task) return service==s_service && user==s_user ? ESP_OK : ESP_ERR_INVALID_STATE;
    s_exited=xSemaphoreCreateBinary();
    if (!s_exited) return ESP_ERR_NO_MEM;
    if (!usb_serial_jtag_is_driver_installed()) {
        /* The driver drops bytes that overflow the ring, so it holds one full line. */
        usb_serial_jtag_driver_config_t config={.rx_buffer_size=PASSPORT_LINE_MAX,.tx_buffer_size=2048};
        esp_err_t e=usb_serial_jtag_driver_install(&config);
        if (e!=ESP_OK) { (void)cleanup(); return e; }
        s_driver_owned=true;
    }
    s_service=service; s_user=user;
    memset(&s_reader,0,sizeof s_reader);
    atomic_store(&s_stop_requested,false);
    /* About 3.9 KB measured after info/get; the rest is margin. */
    if (xTaskCreate(usb_worker,"passport_usb",5632,NULL,4,&s_task)!=pdPASS) {
        s_task=NULL; s_service=NULL; s_user=NULL;
        (void)cleanup();
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t passport_usb_stop(void) {
    if (s_task) {
        if (xTaskGetCurrentTaskHandle()==s_task) return ESP_ERR_INVALID_STATE;
        atomic_store(&s_stop_requested,true);
        if (xSemaphoreTake(s_exited,pdMS_TO_TICKS(2000))!=pdTRUE) return ESP_ERR_TIMEOUT;
        vTaskDelete(s_task); s_task=NULL;
    }
    s_service=NULL; s_user=NULL;
    return cleanup();
}
