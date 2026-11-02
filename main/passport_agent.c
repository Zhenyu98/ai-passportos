#include "passport_agent.h"
#include "bsp_audio.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <string.h>

#define JOB_PAIR (PASSPORT_AGENT_REPAIR+1)
typedef struct {unsigned action,generation;uint32_t consent;char text[PASSPORT_AGENT_NOTE_MAX];} agent_job_t;
static passport_agent_model_t model,view;
static QueueHandle_t jobs;
static SemaphoreHandle_t view_lock,exited;
static TaskHandle_t worker;
static atomic_bool stopping;
static atomic_bool starting;
static atomic_uint api_calls;
static atomic_bool leave_requested;
static atomic_uint input_generation;
static atomic_bool visible,speaking,speech_cancel;
static atomic_uint autoplay_generation;
static const passport_agent_transport_t *transport;

__attribute__((weak)) const passport_agent_transport_t *passport_agent_transport(void) {return NULL;}
static void publish(void) {
    xSemaphoreTake(view_lock,portMAX_DELAY);view=model;xSemaphoreGive(view_lock);
}
static void persist(void) {
    nvs_handle_t h;
    esp_err_t err=nvs_open("passport_agent",NVS_READWRITE,&h);
    if(err==ESP_OK) {
        err=nvs_set_blob(h,"journal",&model.journal,sizeof model.journal);
        if(err==ESP_OK) err=nvs_commit(h);
        nvs_close(h);
    }
    if(err!=ESP_OK) {
        passport_agent_copy(model.detail,sizeof model.detail,"保存失败，重启后可能丢失");model.revision++;
    }
}
static void restore(void) {
    nvs_handle_t h;
    if(nvs_open("passport_agent",NVS_READONLY,&h)!=ESP_OK) return;
    /* Only the worker owns model; the published snapshot may already be read. */
    size_t n=sizeof model.journal;
    if(nvs_get_blob(h,"journal",&model.journal,&n)!=ESP_OK ||
       !passport_agent_restore(&model,&model.journal,n)) {
        memset(&model.journal,0,sizeof model.journal);model.journal.version=1;
    } else passport_agent_copy(model.reply,sizeof model.reply,model.journal.result);
    nvs_close(h);
}
static void error(const char *message) {
    if(!model.generation) model.generation=1;
    passport_agent_event(&model,model.generation,PASSPORT_AGENT_ERROR,message);
}
void passport_agent_set_visible(bool value) {
    atomic_store(&visible,value);
    if(!value) {atomic_store(&speech_cancel,true);atomic_store(&autoplay_generation,0);}
}
static uint8_t level(const int16_t *pcm,size_t frames) {
    uint32_t peak=0;
    for(size_t i=0;i<frames;i++) {int32_t sample=pcm[i];uint32_t magnitude=(uint32_t)(sample<0?-sample:sample);if(magnitude>peak) peak=magnitude;}
    unsigned value=peak*100/16000;return (uint8_t)(value>100?100:value);
}
static bool speech_cancelled(void *arg) {
    (void)arg;
    return atomic_load(&stopping) || atomic_load(&speech_cancel) || !atomic_load(&visible) ||
        atomic_load(&autoplay_generation)!=model.generation;
}
static bool speech_begin(void *arg) {
    (void)arg;
    return !speech_cancelled(NULL) && bsp_audio_init()==ESP_OK && bsp_audio_wake()==ESP_OK &&
        bsp_audio_set_format(PASSPORT_SPEECH_RATE,16,1)==ESP_OK;
}
static bool speech_write(void *arg,const int16_t *pcm,size_t frames) {
    (void)arg;
    if(speech_cancelled(NULL) || bsp_audio_write(pcm,frames*2)!=ESP_OK) return false;
    model.voice=PASSPORT_VOICE_PLAYING;model.level=level(pcm,frames);model.revision++;publish();return true;
}
static void speak_reply(void) {
    if(!passport_speech_configured()) {model.voice=PASSPORT_VOICE_UNCONFIGURED;model.revision++;return;}
    if(!atomic_load(&visible) || atomic_load(&leave_requested) || !model.reply[0] ||
       atomic_load(&autoplay_generation)!=model.generation) return;
    atomic_store(&speech_cancel,false);atomic_store(&speaking,true);
    model.voice=PASSPORT_VOICE_PREPARING;model.level=0;model.voice_detail[0]=0;model.revision++;publish();
    const passport_speech_io_t sink={.begin=speech_begin,.write=speech_write,.cancelled=speech_cancelled};
    passport_speech_result_t result=passport_speech_http(model.reply,&sink);
    if(result==PASSPORT_SPEECH_OK && bsp_audio_drain(speech_cancelled,NULL)!=ESP_OK)
        result=speech_cancelled(NULL)?PASSPORT_SPEECH_CANCELLED:PASSPORT_SPEECH_FAILED;
    (void)bsp_audio_sleep();model.level=0;
    model.voice=result==PASSPORT_SPEECH_OK?PASSPORT_VOICE_DONE:
        result==PASSPORT_SPEECH_CANCELLED?PASSPORT_VOICE_STOPPED:PASSPORT_VOICE_FAILED;
    if(result==PASSPORT_SPEECH_FAILED) passport_agent_copy(model.voice_detail,sizeof model.voice_detail,"朗读失败，文字已保留");
    model.revision++;atomic_store(&speaking,false);
}
static void run(void *arg) {
    (void)arg;
    while(atomic_load(&starting)) vTaskDelay(pdMS_TO_TICKS(1));
    restore();model.voice=passport_speech_configured()?PASSPORT_VOICE_READY:PASSPORT_VOICE_UNCONFIGURED;publish();
    bool recording=false,turn=false;
    uint32_t generation=0;
    int64_t began=0;
    int64_t checked_voice=0;
    int16_t pcm[320];char event[PASSPORT_AGENT_REPLY_MAX];
    while(!atomic_load(&stopping)) {
        if(!turn && !recording && esp_timer_get_time()-checked_voice>2000000) {
            checked_voice=esp_timer_get_time();bool configured=passport_speech_configured();
            if(!configured && model.voice!=PASSPORT_VOICE_UNCONFIGURED) {model.voice=PASSPORT_VOICE_UNCONFIGURED;model.revision++;}
            else if(configured && model.voice==PASSPORT_VOICE_UNCONFIGURED) {model.voice=PASSPORT_VOICE_READY;model.revision++;}
        }
        if(atomic_exchange(&leave_requested,false) && recording) {
            transport->cancel();passport_agent_leave(&model);
            recording=turn=false;bsp_audio_sleep();
        }
        if(transport) {
            uint32_t old=model.generation;
            passport_agent_connection(&model,transport->connection());
            if(model.connection==PASSPORT_AGENT_CONFIRM && transport->confirmation) {
                uint32_t request=transport->confirmation();
                if(model.confirmation!=request) {model.confirmation=request;model.revision++;}
            }
            if(transport->describe) {
                char info[sizeof model.connection_info];transport->describe(info,sizeof info);
                if(strcmp(info,model.connection_info)) {
                    passport_agent_copy(model.connection_info,sizeof model.connection_info,info);model.revision++;
                }
            }
            if(old!=model.generation && turn) {
                transport->cancel();recording=turn=false;bsp_audio_sleep();
            }
        }
        agent_job_t job;
        if(xQueueReceive(jobs,&job,recording?0:pdMS_TO_TICKS(25))==pdTRUE) {
            if((job.action==PASSPORT_AGENT_TOGGLE || job.action==JOB_PAIR) && job.generation!=atomic_load(&input_generation)) {
                /* A queued click from the page that just closed is obsolete. */
            } else if(job.action==JOB_PAIR) {
                if(transport && model.connection==PASSPORT_AGENT_CONFIRM && job.consent && job.consent==model.confirmation)
                    (void)transport->confirm(job.consent);
            } else if(job.action==PASSPORT_AGENT_REPAIR) {
                if(transport && transport->repair) {
                    if(turn) transport->cancel();
                    recording=turn=false;bsp_audio_sleep();transport->repair();
                } else error("Muse 重新配对不可用");
            } else if(job.action==PASSPORT_AGENT_LEAVE) {
                if(recording) {
                    transport->cancel();passport_agent_leave(&model);
                    recording=turn=false;bsp_audio_sleep();
                }
            } else if(job.action==PASSPORT_AGENT_TOGGLE) {
                if(!transport) error("请安装 Muse 接入固件");
                else if(model.connection==PASSPORT_AGENT_CONFIRM) {
                    /* A stale speech click must never become pairing consent. */
                    passport_agent_copy(model.detail,sizeof model.detail,"请查看配对提示后，再按 OK 确认");model.revision++;
                }
                else if(recording) {
                    transport->end();passport_agent_sent(&model);recording=false;bsp_audio_sleep();
                } else if(model.state==PASSPORT_AGENT_WAITING) {
                    passport_agent_copy(model.detail,sizeof model.detail,"Muse 正在处理，请稍候");model.revision++;
                } else if(model.connection==PASSPORT_AGENT_READY) {
                    if(bsp_audio_init()!=ESP_OK || bsp_audio_wake()!=ESP_OK || bsp_audio_set_format(16000,16,1)!=ESP_OK) {
                        error("麦克风不可用");
                    } else if((generation=passport_agent_begin(&model))!=0) {
                        model.voice=passport_speech_configured()?PASSPORT_VOICE_READY:PASSPORT_VOICE_UNCONFIGURED;
                        model.voice_detail[0]=0;model.level=0;
                        atomic_store(&autoplay_generation,atomic_load(&visible)?generation:0);
                        turn=recording=transport->begin();began=esp_timer_get_time();
                        if(!recording) {transport->cancel();bsp_audio_sleep();error("连接失败，请重试");}
                    }
                } else {
                    passport_agent_copy(model.detail,sizeof model.detail,
                        model.connection==PASSPORT_AGENT_UNPAIRED?"在 Muse 手机应用中添加设备":"连接尚未就绪，请稍候");model.revision++;
                }
            } else if(job.action==PASSPORT_AGENT_SAVE_RESULT || job.action==PASSPORT_AGENT_SAVE_TEXT) {
                const char *value=job.action==PASSPORT_AGENT_SAVE_TEXT?job.text:model.journal.result;
                if(passport_agent_save_note(&model,value)) {
                    model.detail[0]=0;persist();
                    if(!model.detail[0]) passport_agent_copy(model.detail,sizeof model.detail,"笔记已保存");
                } else {
                    passport_agent_copy(model.detail,sizeof model.detail,"笔记为空或过长，请用语音保存短笔记");model.revision++;
                }
            }
        }
        if(recording) {
            if(esp_timer_get_time()-began>=(int64_t)PASSPORT_AGENT_RECORD_MS*1000) {
                transport->cancel();passport_agent_leave(&model);recording=turn=false;
                bsp_audio_sleep();error("录音超过三十秒，已取消，请重试");
            } else if(bsp_audio_read(pcm,sizeof pcm)!=ESP_OK) {
                transport->cancel();recording=turn=false;bsp_audio_sleep();error("录音失败，请重试");
            } else {
                model.level=level(pcm,sizeof pcm/sizeof pcm[0]);model.revision++;
                transport->audio(pcm,sizeof pcm/sizeof pcm[0]);
            }
        }
        if(transport && turn) {
            int ev=transport->event(event,sizeof event);
            if(ev==1) {
                passport_agent_copy(model.heard,sizeof model.heard,event);model.revision++;
            } else if(ev==2) {
                /* Partial replies do not enable starting a competing turn. */
                passport_agent_copy(model.reply,sizeof model.reply,event);model.revision++;
            } else if(ev==3) {
                passport_agent_event(&model,generation,PASSPORT_AGENT_REPLY,event);
                persist();turn=false;model.level=0;
                if(model.state==PASSPORT_AGENT_REPLY && generation==model.generation) speak_reply();
            } else if(ev==4) {
                transport->cancel();recording=turn=false;bsp_audio_sleep();
                passport_agent_event(&model,generation,PASSPORT_AGENT_ERROR,event);
            }
        }
        publish();
    }
    if(transport && turn) transport->cancel();
    if(recording) bsp_audio_sleep();
    xSemaphoreGive(exited);vTaskSuspend(NULL);
}
esp_err_t passport_agent_start(const passport_agent_transport_t *ops) {
    if(worker) return atomic_load(&stopping)?ESP_ERR_INVALID_STATE:ESP_OK;
    if(ops && (!ops->connection || !ops->confirm || !ops->begin || !ops->audio ||
               !ops->end || !ops->cancel || !ops->event)) return ESP_ERR_INVALID_ARG;
    atomic_store(&stopping,true);atomic_store(&starting,true);
    transport=ops;passport_agent_model_init(&model);view=model;
    jobs=xQueueCreate(4,sizeof(agent_job_t));view_lock=xSemaphoreCreateMutex();exited=xSemaphoreCreateBinary();
    atomic_store(&leave_requested,false);atomic_store(&input_generation,0);
    atomic_store(&visible,false);atomic_store(&speaking,false);atomic_store(&speech_cancel,false);
    atomic_store(&autoplay_generation,0);
    if(!jobs || !view_lock || !exited || xTaskCreate(run,"passport_agent",8192,NULL,4,&worker)!=pdPASS) {
        if(jobs) vQueueDelete(jobs);
        if(view_lock) vSemaphoreDelete(view_lock);
        if(exited) vSemaphoreDelete(exited);
        jobs=NULL;view_lock=exited=NULL;atomic_store(&starting,false);return ESP_ERR_NO_MEM;
    }
    atomic_store(&stopping,false);atomic_store(&starting,false);
    return ESP_OK;
}
static bool api_finish(bool result) {atomic_fetch_sub(&api_calls,1);return result;}
static bool post_job(unsigned action,const char *text,uint32_t consent) {
    atomic_fetch_add(&api_calls,1);
    if(atomic_load(&stopping) || !worker || action>JOB_PAIR) return api_finish(false);
    if(action==PASSPORT_AGENT_LEAVE) {
        passport_agent_set_visible(false);
        atomic_fetch_add(&input_generation,1);atomic_store(&leave_requested,true);return api_finish(true);
    }
    if(action==PASSPORT_AGENT_TOGGLE && atomic_load(&speaking)) {atomic_store(&speech_cancel,true);return api_finish(true);}
    agent_job_t job={.action=action,.generation=atomic_load(&input_generation),.consent=consent};
    if(text && (strlen(text)>=sizeof job.text || !passport_agent_copy(job.text,sizeof job.text,text))) return api_finish(false);
    return api_finish(xQueueSend(jobs,&job,0)==pdTRUE);
}
bool passport_agent_action(unsigned action,const char *text) {
    return action<=PASSPORT_AGENT_REPAIR && post_job(action,text,0);
}
bool passport_agent_confirm_pairing(uint32_t request) {return request && post_job(JOB_PAIR,NULL,request);}
bool passport_agent_snapshot(passport_agent_model_t *out) {
    atomic_fetch_add(&api_calls,1);
    if(atomic_load(&stopping) || !out || !view_lock || xSemaphoreTake(view_lock,0)!=pdTRUE) return api_finish(false);
    *out=view;xSemaphoreGive(view_lock);return api_finish(true);
}
esp_err_t passport_agent_stop(void) {
    if(!worker) return ESP_OK;
    atomic_store(&stopping,true);
    for(unsigned i=0;atomic_load(&api_calls) && i<100;i++) vTaskDelay(pdMS_TO_TICKS(1));
    if(atomic_load(&api_calls)) return ESP_ERR_TIMEOUT;
    if(xSemaphoreTake(exited,pdMS_TO_TICKS(2000))!=pdTRUE) return ESP_ERR_TIMEOUT;
    vTaskDelete(worker);worker=NULL;
    vQueueDelete(jobs);vSemaphoreDelete(view_lock);vSemaphoreDelete(exited);
    jobs=NULL;view_lock=exited=NULL;return ESP_OK;
}
