#include "passport_speech.h"
#include "esp_http_client.h"
#include "nvs.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include <stdlib.h>
#include "freertos/task.h"
#include "freertos/semphr.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <pthread.h>
#include <semaphore.h>
#include <time.h>
#include <errno.h>
#endif

static passport_speech_config_t disk;
static esp_http_client_config_t client;
static unsigned scenario,cleaned,position,samples,begun,sent,requests;
static bool stored;
static atomic_bool socket_aborted,want_cancel;
static atomic_uint clock_offset;
static unsigned shutdowns;
typedef struct {TaskFunction_t fn;void *arg;} thread_arg_t;
#ifdef _WIN32
static DWORD WINAPI enter_thread(void *p) {thread_arg_t copy=*(thread_arg_t *)p;free(p);copy.fn(copy.arg);return 0;}
BaseType_t xTaskCreate(TaskFunction_t f,const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *out) {
    (void)name;(void)stack;(void)priority;thread_arg_t *p=malloc(sizeof *p);p->fn=f;p->arg=arg;
    *out=CreateThread(NULL,0,enter_thread,p,0,NULL);return *out?pdPASS:pdFALSE;
}
void vTaskDelay(TickType_t ticks) {Sleep(ticks);}
void vTaskSuspend(TaskHandle_t task) {(void)task;ExitThread(0);}
void vTaskDelete(TaskHandle_t task) {WaitForSingleObject(task,3000);CloseHandle(task);}
SemaphoreHandle_t xSemaphoreCreateBinary(void) {return CreateSemaphore(NULL,0,1,NULL);}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {return CreateSemaphore(NULL,1,1,NULL);}
BaseType_t xSemaphoreTake(SemaphoreHandle_t h,TickType_t timeout) {return WaitForSingleObject(h,timeout)==WAIT_OBJECT_0?pdTRUE:pdFALSE;}
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) {return ReleaseSemaphore(h,1,NULL)?pdTRUE:pdFALSE;}
void vSemaphoreDelete(SemaphoreHandle_t h) {CloseHandle(h);}
int64_t esp_timer_get_time(void) {return (int64_t)(GetTickCount64()+atomic_load(&clock_offset))*1000;}
#else
static void *enter_thread(void *p) {thread_arg_t copy=*(thread_arg_t *)p;free(p);copy.fn(copy.arg);return NULL;}
BaseType_t xTaskCreate(TaskFunction_t f,const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *out) {
    (void)name;(void)stack;(void)priority;thread_arg_t *p=malloc(sizeof *p);p->fn=f;p->arg=arg;
    pthread_t *thread=malloc(sizeof *thread);if(pthread_create(thread,NULL,enter_thread,p)) {free(thread);free(p);return pdFALSE;}*out=thread;return pdPASS;
}
void vTaskDelay(TickType_t ticks) {struct timespec t={ticks/1000,(ticks%1000)*1000000L};nanosleep(&t,NULL);}
void vTaskSuspend(TaskHandle_t task) {(void)task;pthread_exit(NULL);}
void vTaskDelete(TaskHandle_t task) {pthread_join(*(pthread_t *)task,NULL);free(task);}
SemaphoreHandle_t xSemaphoreCreateBinary(void) {sem_t *s=malloc(sizeof *s);sem_init(s,0,0);return s;}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {sem_t *s=malloc(sizeof *s);sem_init(s,0,1);return s;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t h,TickType_t timeout) {
    if(timeout==portMAX_DELAY)return sem_wait(h)==0?pdTRUE:pdFALSE;
    for(TickType_t t=0;t<timeout;t++) {if(!sem_trywait(h))return pdTRUE;vTaskDelay(1);}return pdFALSE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) {return sem_post(h)==0?pdTRUE:pdFALSE;}
void vSemaphoreDelete(SemaphoreHandle_t h) {sem_destroy(h);free(h);}
int64_t esp_timer_get_time(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return ((int64_t)t.tv_sec*1000+t.tv_nsec/1000000+atomic_load(&clock_offset))*1000;}
#endif
int shutdown(int fd,int how) {assert(fd==42 && how==2);shutdowns++;atomic_store(&socket_aborted,true);return 0;}
int esp_http_client_get_socket(esp_http_client_handle_t h) {(void)h;return 42;}
static void slow_network(bool header) {
    if(scenario==9 || scenario==10)atomic_store(&want_cancel,true);
    else atomic_store(&clock_offset,header?11000:151000);
    for(unsigned i=0;i<100 && !atomic_load(&socket_aborted);i++)vTaskDelay(10);
}
esp_err_t esp_crt_bundle_attach(void *p) {(void)p;return ESP_OK;}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h) {assert(!strcmp(ns,"passport_voice"));*h=1;return !stored&&mode==NVS_READONLY?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *size) {(void)h;assert(!strcmp(key,"config") && *size==sizeof disk);memcpy(out,&disk,sizeof disk);return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t size) {(void)h;assert(!strcmp(key,"config") && size==sizeof disk);memcpy(&disk,data,size);stored=true;return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {(void)h;return ESP_OK;}
void nvs_close(nvs_handle_t h) {(void)h;}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t *cfg) {
    assert(cfg->disable_auto_redirect && cfg->timeout_ms<=1000 && cfg->crt_bundle_attach);
    assert(!strcmp(cfg->url,"http://192.168.1.2:8765/tts"));client=*cfg;requests++;return &client;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t h,const char *key,const char *value) {
    (void)h;
    if(!strcmp(key,"Authorization")) assert(!strcmp(value,"Bearer 0123456789abcdef"));return ESP_OK;
}
esp_err_t esp_http_client_open(esp_http_client_handle_t h,int size) {
    (void)h;assert(size==6);
    esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_CONNECTED,.user_data=client.user_data};client.event_handler(&e);
    return scenario==1?ESP_FAIL:ESP_OK;
}
int esp_http_client_write(esp_http_client_handle_t h,const char *data,int size) {(void)h;(void)data;int n=size>3?3:size;sent+=(unsigned)n;return n;}
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t h) {
    (void)h;assert(sent==6);
    if(scenario==9 || scenario==11) {slow_network(true);return -1;}
    esp_http_client_event_t e={.event_id=HTTP_EVENT_ON_HEADER,.user_data=client.user_data,
        .header_key="x-passport-audio",.header_value=scenario==2?"wrong format":"pcm_s16le_16000_mono"};
    if(scenario!=3)client.event_handler(&e);
    if(scenario==4)client.event_handler(&e);
    return scenario==5?4000000:4;
}
int esp_http_client_get_status_code(esp_http_client_handle_t h) {(void)h;return scenario==6?302:200;}
int esp_http_client_read(esp_http_client_handle_t h,char *out,int size) {
    (void)h;assert(size>=3);
    if(scenario==10 || scenario==12) {slow_network(false);return -1;}
    const char data[]={1,0,2,0};
    if(position>=4)return 0;
    unsigned n=position?1:3;memcpy(out,data+position,n);position+=n;return (int)n;
}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t h) {(void)h;return scenario!=7;}
esp_err_t esp_http_client_close(esp_http_client_handle_t h) {(void)h;return ESP_OK;}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t h) {(void)h;cleaned++;return ESP_OK;}
static bool cancel(void *p) {(void)p;return scenario==8 || atomic_load(&want_cancel);}
static bool begin(void *p) {(void)p;begun++;return true;}
static bool write_audio(void *p,const int16_t *pcm,size_t n) {(void)p;for(size_t i=0;i<n;i++)assert(pcm[i]==(int16_t)++samples);return true;}
int main(void) {
    assert(!passport_speech_configured());
    const char *line=">voice_config={\"url\":\"http://192.168.1.2:8765/tts\",\"token\":\"0123456789abcdef\"}";
    assert(passport_speech_configuration_line(line,strlen(line)) && passport_speech_configured());
    passport_speech_io_t sink={.begin=begin,.write=write_audio,.cancelled=cancel};
    for(scenario=0;scenario<=12;scenario++) {
        cleaned=position=samples=begun=sent=requests=0;
        shutdowns=0;atomic_store(&socket_aborted,false);atomic_store(&want_cancel,false);atomic_store(&clock_offset,0);
        passport_speech_result_t result=passport_speech_http("你好",&sink);
        if(!scenario) assert(result==PASSPORT_SPEECH_OK && samples==2 && begun==1);
        else if(scenario==8) assert(result==PASSPORT_SPEECH_CANCELLED && !requests && !samples);
        else if(scenario==9 || scenario==10) assert(result==PASSPORT_SPEECH_CANCELLED && shutdowns==1 && !samples);
        else if(scenario==11 || scenario==12) assert(result==PASSPORT_SPEECH_FAILED && shutdowns==1 && !samples);
        else assert(result==PASSPORT_SPEECH_FAILED && (scenario==7 || !samples));
        assert(cleaned==requests);
    }
    puts("Speech HTTP auth, response format, partial reads, EOF, redirects and cleanup: PASS");
}
