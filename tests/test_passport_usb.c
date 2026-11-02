#ifdef _WIN32
#define flockfile _lock_file
#define funlockfile _unlock_file
#endif
#include "../main/passport_usb.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <string.h>

static bool fail_semaphore,fail_task,fail_install,driver_installed,ack,in_worker;
static unsigned sem_created,sem_deleted,task_deleted,driver_uninstalled,service_calls;
static int semaphore_token,task_token;
static TaskFunction_t saved_worker;
static jmp_buf worker_exit;
static const char *incoming;
static char outgoing[PASSPORT_LINE_MAX];
static size_t outgoing_size;
static unsigned voice_commands;
bool passport_speech_configuration_line(const char *line,size_t length) {
    if(length>=14 && !strncmp(line,">voice_config=",14)) {voice_commands++;return true;}
    return false;
}

SemaphoreHandle_t xSemaphoreCreateBinary(void) { if(fail_semaphore)return NULL; ++sem_created; return &semaphore_token; }
BaseType_t xSemaphoreTake(SemaphoreHandle_t s,TickType_t ticks) { (void)ticks; assert(s==&semaphore_token); bool value=ack; ack=false; return value; }
BaseType_t xSemaphoreGive(SemaphoreHandle_t s) { assert(s==&semaphore_token); ack=true; return pdTRUE; }
void vSemaphoreDelete(SemaphoreHandle_t s) { assert(s==&semaphore_token); ++sem_deleted; }
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *out) {
    (void)name;(void)arg;(void)priority; assert(stack>=4096);
    if(fail_task)return pdFALSE;
    saved_worker=fn; *out=&task_token; return pdPASS;
}
TaskHandle_t xTaskGetCurrentTaskHandle(void) { return in_worker?&task_token:NULL; }
void vTaskDelete(TaskHandle_t t) { assert(t==&task_token); ++task_deleted; }
void vTaskSuspend(TaskHandle_t t) { assert(t==NULL); longjmp(worker_exit,1); }
void vTaskDelay(TickType_t ticks) { (void)ticks; }
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t *config) {
    assert(config->rx_buffer_size>0 && config->tx_buffer_size>0);
    if(fail_install)return ESP_FAIL;
    driver_installed=true; return ESP_OK;
}
esp_err_t usb_serial_jtag_driver_uninstall(void) { driver_installed=false; ++driver_uninstalled; return ESP_OK; }
bool usb_serial_jtag_is_driver_installed(void) { return driver_installed; }
void usb_serial_jtag_vfs_use_driver(void) {}
void usb_serial_jtag_vfs_use_nonblocking(void) {}
int usb_serial_jtag_read_bytes(void *out,uint32_t capacity,TickType_t ticks) {
    (void)ticks;
    if(!incoming || !*incoming) { atomic_store(&s_stop_requested,true); return 0; }
    size_t n=strlen(incoming); if(n>capacity)n=capacity;
    memcpy(out,incoming,n); incoming+=n; return (int)n;
}
int usb_serial_jtag_write_bytes(const void *data,size_t size,TickType_t ticks) {
    (void)ticks; assert(outgoing_size+size<sizeof outgoing);
    memcpy(outgoing+outgoing_size,data,size); outgoing_size+=size; outgoing[outgoing_size]=0;
    return (int)size;
}
esp_err_t usb_serial_jtag_wait_tx_done(TickType_t ticks) { assert(ticks>0); return ESP_OK; }
static passport_status_t service(const passport_request_t *request,passport_reply_t *reply,void *user) {
    (void)user; ++service_calls;
    assert(request->op==PASSPORT_OP_GET);
    reply->record.schema=1; reply->record.brightness=60;
    strcpy(reply->record.profile.name,"Alice"); return PASSPORT_OK;
}
int main(void) {
    assert(passport_usb_start(NULL,NULL)==ESP_ERR_INVALID_ARG);
    fail_semaphore=true;
    assert(passport_usb_start(service,NULL)==ESP_ERR_NO_MEM && sem_created==0);
    fail_semaphore=false; fail_install=true;
    assert(passport_usb_start(service,NULL)==ESP_FAIL && sem_created==sem_deleted);
    fail_install=false; fail_task=true;
    assert(passport_usb_start(service,NULL)==ESP_ERR_NO_MEM && !driver_installed);
    assert(sem_created==sem_deleted && driver_uninstalled==1);
    driver_installed=true;
    assert(passport_usb_start(service,NULL)==ESP_ERR_NO_MEM);
    assert(driver_installed && driver_uninstalled==1);
    driver_installed=false; fail_task=false;
    assert(passport_usb_start(service,NULL)==ESP_OK);
    unsigned created=sem_created;
    assert(passport_usb_start(service,NULL)==ESP_OK && sem_created==created);
    incoming=">voice_config={}\n{\"id\":4,\"op\":\"get\"}\n"; in_worker=true;
    if(!setjmp(worker_exit)) saved_worker(NULL);
    in_worker=false;
    assert(voice_commands==1 && service_calls==1 && strstr(outgoing,"PASSPORT/1 ") && strstr(outgoing,"Alice"));
    assert(passport_usb_stop()==ESP_OK && !driver_installed);
    assert(sem_created==sem_deleted && task_deleted==1);
    assert(passport_usb_start(service,NULL)==ESP_OK);
    ack=false;
    assert(passport_usb_stop()==ESP_ERR_TIMEOUT);
    assert(driver_installed && sem_created!=sem_deleted);
    ack=true;
    assert(passport_usb_stop()==ESP_OK && sem_created==sem_deleted);
    puts("Passport USB lifecycle: PASS");
    return 0;
}
