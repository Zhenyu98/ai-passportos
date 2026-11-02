/* Execute the real worker with deterministic RTOS, audio and transport fakes. */
#include "../main/passport_agent.c"
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {unsigned cap,size,count,head;unsigned char *data;} queue_t;
typedef struct {bool value;} sem_t;
static jmp_buf parked;
static TaskFunction_t entry;
static unsigned step,scenario,reads,begins,ends,cancels,live_tasks,confirmations,repairs;
static bool stored,commit_fail,audio_sleeping;
static unsigned spoken,play_writes;
static unsigned queued_audio,played_audio,drains,clipped;
static bool voice_configured;
static passport_agent_journal_t disk;
QueueHandle_t xQueueCreate(UBaseType_t cap,UBaseType_t size) {
    queue_t *q=calloc(1,sizeof *q);q->cap=cap;q->size=size;q->data=calloc(cap,size);return q;
}
BaseType_t xQueueSend(QueueHandle_t h,const void *data,TickType_t ms) {
    (void)ms;queue_t *q=h;if(q->count==q->cap)return pdFALSE;
    memcpy(q->data+(q->head+q->count)%q->cap*q->size,data,q->size);q->count++;return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t h,void *data,TickType_t ms) {
    (void)ms;step++;
    if(scenario==1) {
        if(step==6) assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        if(step==7) assert(passport_agent_action(PASSPORT_AGENT_LEAVE,NULL));
    } else if(scenario==2 || scenario==3) {
        if(step==1 || (scenario==2 && step==2)) assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        if(step==3) assert(passport_agent_action(PASSPORT_AGENT_LEAVE,NULL));
    } else if(scenario==4 && step==1) assert(passport_agent_action(PASSPORT_AGENT_SAVE_TEXT,"persist me"));
    else if(scenario==6) {
        if(step==1 || step==2 || step==5) assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        if(step==6) assert(passport_agent_action(PASSPORT_AGENT_LEAVE,NULL));
    } else if(scenario==7) {
        if(step==1) assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        if(step==2) assert(passport_agent_confirm_pairing(78));
        if(step==3) assert(passport_agent_confirm_pairing(77));
    } else if(scenario==8 && step==1) {
        assert(passport_agent_action(PASSPORT_AGENT_REPAIR,NULL));
    } else if(scenario>=9 && scenario<=13) {
        if(step==1 || step==2) assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        if(scenario==10 && step==3) passport_agent_set_visible(false);
        if(scenario==13 && step==3) {passport_agent_set_visible(false);passport_agent_set_visible(true);}
    }
    if(step==10) atomic_store(&stopping,true);
    queue_t *q=h;if(!q->count)return pdFALSE;
    memcpy(data,q->data+q->head*q->size,q->size);q->head=(q->head+1)%q->cap;q->count--;return pdTRUE;
}
void vQueueDelete(QueueHandle_t h) {queue_t *q=h;free(q->data);free(q);}
SemaphoreHandle_t xSemaphoreCreateBinary(void) {return calloc(1,sizeof(sem_t));}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {sem_t *s=xSemaphoreCreateBinary();s->value=true;return s;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t h,TickType_t ms) {(void)ms;sem_t *s=h;if(!s->value)return pdFALSE;s->value=false;return pdTRUE;}
BaseType_t xSemaphoreGive(SemaphoreHandle_t h) {((sem_t *)h)->value=true;return pdTRUE;}
void vSemaphoreDelete(SemaphoreHandle_t h) {free(h);}
BaseType_t xTaskCreate(TaskFunction_t f,const char *n,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *h) {
    (void)n;(void)stack;(void)arg;(void)priority;entry=f;*h=(void *)1;live_tasks++;return pdPASS;
}
void vTaskDelete(TaskHandle_t h) {(void)h;live_tasks--;}
void vTaskSuspend(TaskHandle_t h) {(void)h;longjmp(parked,1);}
void vTaskDelay(TickType_t ms) {(void)ms;}
int64_t esp_timer_get_time(void) {return step*20000;}
esp_err_t bsp_audio_init(void) {return ESP_OK;}
esp_err_t bsp_audio_set_format(uint32_t hz,uint8_t bits,uint8_t channels) {assert(hz==16000 && bits==16 && channels==1);return audio_sleeping?ESP_ERR_INVALID_STATE:ESP_OK;}
esp_err_t bsp_audio_wake(void) {audio_sleeping=false;return ESP_OK;}
esp_err_t bsp_audio_read(void *pcm,size_t n) {memset(pcm,0,n);reads++;return ESP_OK;}
esp_err_t bsp_audio_sleep(void) {if(queued_audio)clipped++;queued_audio=0;audio_sleeping=true;return ESP_OK;}
esp_err_t bsp_audio_drain(bool (*cancel)(void *),void *arg) {
    assert(!cancel(arg));drains++;played_audio+=queued_audio;queued_audio=0;return ESP_OK;
}
esp_err_t nvs_open(const char *ns,int mode,nvs_handle_t *h) {(void)ns;*h=1;return mode==NVS_READONLY&&!stored?ESP_ERR_NVS_NOT_FOUND:ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char *key,void *out,size_t *n) {(void)h;(void)key;assert(*n==sizeof disk);memcpy(out,&disk,sizeof disk);return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char *key,const void *data,size_t n) {(void)h;(void)key;assert(n==sizeof disk);if(!commit_fail){memcpy(&disk,data,n);stored=true;}return ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h) {(void)h;return commit_fail?ESP_FAIL:ESP_OK;}
void nvs_close(nvs_handle_t h) {(void)h;}
static passport_agent_state_t connection_state(void) {
    if(scenario==7) return PASSPORT_AGENT_CONFIRM;
    return scenario==3&&step>=2?PASSPORT_AGENT_OFFLINE:PASSPORT_AGENT_READY;
}
static bool confirm(uint32_t request) {assert(request==77);confirmations++;return true;}
static uint32_t pending(void) {return 77;}
static void repair(void) {repairs++;}
static bool begin_turn(void) {begins++;return true;}
static void audio(const int16_t *p,size_t n) {(void)p;assert(n==320);}
static void end_turn(void) {ends++;}
static void cancel_turn(void) {cancels++;}
static int next_event(char *out,size_t cap) {
    if(scenario>=9 && scenario<=13 && step==4) {snprintf(out,cap,"spoken reply");return 3;}
    if(scenario==2 && (step==5 || step==6)) {snprintf(out,cap,"finished task");return step==5?2:3;}
    if(scenario==6 && step==4) {snprintf(out,cap,"first turn");return 3;}
    return 0;
}
bool passport_speech_configured(void) {return voice_configured;}
esp_err_t bsp_audio_write(const void *pcm,size_t n) {assert(pcm && n && !audio_sleeping);play_writes++;queued_audio+=(unsigned)n;return ESP_OK;}
passport_speech_result_t passport_speech_http(const char *text,const passport_speech_io_t *sink) {
    assert(!strcmp(text,"spoken reply"));spoken++;
    if(scenario==11) return PASSPORT_SPEECH_FAILED;
    assert(sink->begin(sink->context));
    const int16_t pcm[]={1000,-1000};assert(sink->write(sink->context,pcm,2));
    if(scenario==12) {
        assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        assert(sink->cancelled(sink->context));return PASSPORT_SPEECH_CANCELLED;
    }
    return PASSPORT_SPEECH_OK;
}
static const passport_agent_transport_t ops={.connection=connection_state,.confirm=confirm,.begin=begin_turn,
    .audio=audio,.end=end_turn,.cancel=cancel_turn,.event=next_event,.confirmation=pending,.repair=repair};
static void execute(unsigned which) {
    step=reads=begins=ends=cancels=confirmations=repairs=spoken=play_writes=0;scenario=which;audio_sleeping=false;
    queued_audio=played_audio=drains=clipped=0;
    voice_configured=which>=9;
    assert(passport_agent_start(&ops)==ESP_OK);
    passport_agent_set_visible(true);
    if(which==1) {
        for(unsigned i=0;i<4;i++) assert(passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        assert(!passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
        assert(passport_agent_action(PASSPORT_AGENT_LEAVE,NULL));
    }
    if(!setjmp(parked)) entry(NULL);
    assert(passport_agent_stop()==ESP_OK && !worker && !jobs && !view_lock && !live_tasks);
}
int main(void) {
    execute(1);assert(begins==1 && ends==0 && cancels==1 && reads<=2);
    execute(2);assert(begins==1 && ends==1 && cancels==0);
    assert(stored && !strcmp(disk.result,"finished task"));
    execute(3);assert(cancels==1 && model.state==PASSPORT_AGENT_ERROR);
    execute(6);assert(begins==2 && ends==1 && cancels==1);
    execute(7);assert(confirmations==1 && !begins);
    execute(8);assert(repairs==1);
    execute(9);assert(spoken==1 && play_writes==1 && model.voice==PASSPORT_VOICE_DONE && audio_sleeping);
    assert(drains==1 && played_audio==4 && !clipped);
    execute(10);assert(!spoken && model.state==PASSPORT_AGENT_REPLY);
    execute(11);assert(spoken==1 && model.voice==PASSPORT_VOICE_FAILED && !strcmp(model.reply,"spoken reply"));
    execute(12);assert(spoken==1 && model.voice==PASSPORT_VOICE_STOPPED && begins==1 && audio_sleeping);
    assert(!drains);
    execute(13);assert(!spoken && model.state==PASSPORT_AGENT_REPLY);
    commit_fail=true;execute(4);assert(strstr(model.detail,"保存失败"));
    assert(passport_agent_start(&ops)==ESP_OK);
    assert(passport_agent_stop()==ESP_ERR_TIMEOUT && worker && jobs && view_lock);
    assert(!passport_agent_action(PASSPORT_AGENT_TOGGLE,NULL));
    passport_agent_model_t snapshot;
    assert(!passport_agent_snapshot(&snapshot));
    if(!setjmp(parked)) entry(NULL);
    assert(passport_agent_stop()==ESP_OK && !live_tasks);
    puts("Agent worker cancellation, persistence, reconnect and shutdown: PASS");return 0;
}
