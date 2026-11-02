#include "passport_speech.h"
#include "passport_agent_model.h"
#include "esp_http_client.h"
#include "esp_crt_bundle.h"
#include "esp_timer.h"
#include "nvs.h"
#include "cJSON.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "lwip/sockets.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>

static bool load(passport_speech_config_t *c) {
    nvs_handle_t h;memset(c,0,sizeof *c);
    if(nvs_open("passport_voice",NVS_READONLY,&h)!=ESP_OK) return false;
    size_t size=sizeof *c;
    esp_err_t e=nvs_get_blob(h,"config",c,&size);nvs_close(h);
    return e==ESP_OK && size==sizeof *c && passport_speech_config_valid(c);
}
bool passport_speech_configured(void) {passport_speech_config_t c;return load(&c);}
bool passport_speech_configuration_line(const char *line,size_t length) {
    static const char prefix[]=">voice_config=";
    if(length<sizeof prefix-1 || strncmp(line,prefix,sizeof prefix-1)) return false;
    cJSON *root=cJSON_Parse(line+sizeof prefix-1);
    passport_speech_config_t c={0};bool ok=false;
    if(root && cJSON_IsObject(root)) {
        const cJSON *url=cJSON_GetObjectItemCaseSensitive(root,"url"),*token=cJSON_GetObjectItemCaseSensitive(root,"token");
        if(cJSON_IsString(url) && cJSON_IsString(token) && strlen(url->valuestring)<sizeof c.url && strlen(token->valuestring)<sizeof c.token) {
            strcpy(c.url,url->valuestring);strcpy(c.token,token->valuestring);
            /* Empty pair explicitly disables speech; pairing/profile are untouched. */
            if((!c.url[0] && !c.token[0]) || passport_speech_config_valid(&c)) {
                nvs_handle_t h;
                if(nvs_open("passport_voice",NVS_READWRITE,&h)==ESP_OK) {
                    ok=nvs_set_blob(h,"config",&c,sizeof c)==ESP_OK && nvs_commit(h)==ESP_OK;nvs_close(h);
                }
            }
        }
    }
    cJSON_Delete(root);memset(&c,0,sizeof c);
    printf("@voice_config %s\n",ok?"ok":"error");fflush(stdout);return true;
}
typedef struct {
    esp_http_client_handle_t client;const passport_speech_io_t *sink;
    unsigned formats;bool format_ok;
    SemaphoreHandle_t socket_lock,guard_exited;
    TaskHandle_t guard;
    int socket;
    uint32_t started;
    atomic_uint last_rx;
    atomic_bool stop,aborted,headers_done;
} http_voice_t;
/* ESP HTTP may loop internally while a peer trickles bytes. A separate guard
 * owns no audio and only shuts down the registered socket. Cleanup joins it
 * before releasing the client/context, preventing stale-FD reuse and UAF. */
static void guard_socket(void *p) {
    http_voice_t *v=p;
    while(!atomic_load(&v->stop)) {
        uint32_t now=(uint32_t)(esp_timer_get_time()/1000);
        bool expired=now-v->started>150000u ||
            (!atomic_load(&v->headers_done) && now-v->started>10000u) ||
            (atomic_load(&v->headers_done) && now-atomic_load(&v->last_rx)>10000u);
        if(expired || v->sink->cancelled(v->sink->context)) {
            xSemaphoreTake(v->socket_lock,portMAX_DELAY);
            if(!atomic_exchange(&v->aborted,true) && v->socket>=0) shutdown(v->socket,SHUT_RDWR);
            xSemaphoreGive(v->socket_lock);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
    xSemaphoreGive(v->guard_exited);vTaskSuspend(NULL);
}
static esp_err_t response_header(esp_http_client_event_t *e) {
    http_voice_t *v=e->user_data;
    if(e->event_id==HTTP_EVENT_ON_CONNECTED) {
        xSemaphoreTake(v->socket_lock,portMAX_DELAY);
        v->socket=esp_http_client_get_socket(v->client);
        if(atomic_load(&v->aborted) && v->socket>=0) shutdown(v->socket,SHUT_RDWR);
        xSemaphoreGive(v->socket_lock);
    }
    if(e->event_id==HTTP_EVENT_ON_HEADER || e->event_id==HTTP_EVENT_ON_DATA)
        atomic_store(&v->last_rx,(unsigned)(esp_timer_get_time()/1000));
    if(e->event_id==HTTP_EVENT_ON_HEADER && !strcasecmp(e->header_key,"X-Passport-Audio")) {
        v->formats++;v->format_ok=!strcmp(e->header_value,"pcm_s16le_16000_mono");
    }
    return ESP_OK;
}
static bool cancelled(void *p) {http_voice_t *v=p;return v->sink->cancelled(v->sink->context);}
static bool open_source(void *p,const char *text) {
    http_voice_t *v=p;passport_speech_config_t c;
    if(!load(&c) || cancelled(p)) return false;
    esp_http_client_config_t cfg={.url=c.url,.method=HTTP_METHOD_POST,.timeout_ms=800,
        .crt_bundle_attach=esp_crt_bundle_attach,.buffer_size=1024,.buffer_size_tx=1024,.disable_auto_redirect=true,
        .event_handler=response_header,.user_data=v};
    v->client=esp_http_client_init(&cfg);
    if(!v->client) return false;
    v->socket=-1;v->socket_lock=xSemaphoreCreateMutex();v->guard_exited=xSemaphoreCreateBinary();
    v->started=(uint32_t)(esp_timer_get_time()/1000);atomic_store(&v->last_rx,v->started);
    if(!v->socket_lock || !v->guard_exited ||
       xTaskCreate(guard_socket,"speech_guard",2048,v,5,&v->guard)!=pdPASS) return false;
    char bearer[sizeof c.token+8];snprintf(bearer,sizeof bearer,"Bearer %s",c.token);
    esp_http_client_set_header(v->client,"Authorization",bearer);
    esp_http_client_set_header(v->client,"Content-Type","text/plain; charset=utf-8");
    esp_http_client_set_header(v->client,"Accept","audio/pcm");
    esp_http_client_set_header(v->client,"Accept-Encoding","identity");
    memset(&c,0,sizeof c);memset(bearer,0,sizeof bearer);
    size_t size=strlen(text),sent=0;
    if(cancelled(p) || esp_http_client_open(v->client,(int)size)!=ESP_OK) return false;
    while(sent<size && !cancelled(p)) {
        int n=esp_http_client_write(v->client,text+sent,(int)(size-sent));
        if(n<=0) return false;
        sent+=(size_t)n;
    }
    int64_t began=esp_timer_get_time(),length;
    do {
        if(cancelled(p) || atomic_load(&v->aborted) || esp_timer_get_time()-began>10000000) return false;
        length=esp_http_client_fetch_headers(v->client);
    } while(length==-ESP_ERR_HTTP_EAGAIN);
    if(length<0 || esp_http_client_get_status_code(v->client)!=200) return false;
    atomic_store(&v->headers_done,true);atomic_store(&v->last_rx,(unsigned)(esp_timer_get_time()/1000));
    if(length>(int64_t)PASSPORT_SPEECH_RATE*2*PASSPORT_SPEECH_SECONDS) return false;
    return v->formats==1 && v->format_ok;
}
static int read_source(void *p,uint8_t *out,size_t size) {
    http_voice_t *v=p;int n=esp_http_client_read(v->client,(char *)out,(int)size);
    if(atomic_load(&v->aborted)) return -1;
    if(n==-ESP_ERR_HTTP_EAGAIN) return -2;
    if(!n && !esp_http_client_is_complete_data_received(v->client)) return -1;
    return n;
}
static void close_source(void *p) {
    http_voice_t *v=p;
    if(v->guard) {
        atomic_store(&v->stop,true);xSemaphoreTake(v->guard_exited,portMAX_DELAY);
        vTaskDelete(v->guard);v->guard=NULL;
    }
    if(v->client) {esp_http_client_close(v->client);esp_http_client_cleanup(v->client);v->client=NULL;}
    if(v->socket_lock) {vSemaphoreDelete(v->socket_lock);v->socket_lock=NULL;}
    if(v->guard_exited) {vSemaphoreDelete(v->guard_exited);v->guard_exited=NULL;}
}
static bool begin(void *p) {http_voice_t *v=p;return v->sink->begin(v->sink->context);}
static bool write_audio(void *p,const int16_t *data,size_t frames) {http_voice_t *v=p;return v->sink->write(v->sink->context,data,frames);}
static uint32_t now(void *p) {(void)p;return (uint32_t)(esp_timer_get_time()/1000);}
passport_speech_result_t passport_speech_http(const char *text,const passport_speech_io_t *sink) {
    http_voice_t v={.sink=sink};
    passport_speech_io_t io={.context=&v,.open=open_source,.read=read_source,.close=close_source,
        .begin=begin,.write=write_audio,.cancelled=cancelled,.now_ms=now};
    return passport_speech_play(&io,text);
}
