#include "passport_app.h"
#include "passport_assets.h"
#include "passport_avatar_flash.h"
#include "passport_config.h"
#include "passport_nvs.h"
#include "passport_protocol.h"
#include "passport_ui.h"
#include "passport_usb.h"
#include "passport_strings.h"
#include "passport_version.h"
#include "passport_agent.h"
#include "bsp_button.h"
#include "bsp_display.h"
#include "bsp_battery.h"
#include "bsp_pins.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_heap_caps.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>

typedef struct {passport_key_t key;passport_key_event_t event;uint64_t now;unsigned control,value;} input_t;
/* Both transports use the same owned request slot and acknowledgement. */
typedef struct {uint8_t kind;uint8_t brightness;} job_t;
typedef struct {passport_record_t record;passport_status_t status;bool time_valid;} update_t;
static QueueHandle_t input_queue,core_queue,updates;
static SemaphoreHandle_t core_exited,ui_exited,done,service_mutex;
static TaskHandle_t core_task,ui_task;
static atomic_bool running,stopping,input_ready,input_lost,clock_synced;
static atomic_uint callback_count;
static passport_model_t model;
static passport_record_t ui_record,core_record;
static passport_store_backend_t backend;
static bool storage_ready,battery_ready,time_valid,ui_time_valid,wait_release,ui_created;
static bool pending;
static bool game_input_active;
static passport_request_t owned_request;
static passport_reply_t owned_reply;
static const char *startup_status;
static int cached_battery=-1;
static atomic_int public_battery;
static atomic_uint public_brightness,public_page;
static passport_agent_model_t agent_snapshot;
static uint32_t pressed_confirmation;

void passport_app_device_status(int *battery,unsigned *brightness,passport_page_t *page) {
    if(battery) *battery=atomic_load(&public_battery);
    if(brightness) *brightness=atomic_load(&public_brightness);
    if(page) *page=(passport_page_t)atomic_load(&public_page);
}
/* Each owner folds this sticky flag into its own copy of time validity. */
void passport_app_clock_synced(void) {atomic_store(&clock_synced,true);}
bool passport_app_remote_brightness(unsigned percent) {
    bool sent=false;atomic_fetch_add(&callback_count,1);
    if(atomic_load(&input_ready) && (percent==30 || percent==60 || percent==90)) {
        input_t input={.control=1,.value=percent};sent=xQueueSend(input_queue,&input,0)==pdTRUE;
    }
    atomic_fetch_sub(&callback_count,1);return sent;
}
bool passport_app_remote_page(passport_page_t page) {
    bool sent=false;atomic_fetch_add(&callback_count,1);
    if(atomic_load(&input_ready) && page<=PASSPORT_PAGE_NOTES && page>=0 && page!=PASSPORT_PAGE_GAME) {
        input_t input={.control=2,.value=(unsigned)page};sent=xQueueSend(input_queue,&input,0)==pdTRUE;
    }
    atomic_fetch_sub(&callback_count,1);return sent;
}


bool passport_app_post_input(passport_key_t key,passport_key_event_t event,uint64_t now) {
    bool sent=false;
    atomic_fetch_add(&callback_count,1);
    if(atomic_load(&input_ready) && key>=PASSPORT_KEY_UP && key<=PASSPORT_KEY_OK &&
       (event==PASSPORT_KEY_PRESS || event==PASSPORT_KEY_RELEASE)) {
        input_t input={.key=key,.event=event,.now=now};
        sent=xQueueSend(input_queue,&input,0)==pdTRUE;
        if(!sent) atomic_store(&input_lost,true);
    }
    atomic_fetch_sub(&callback_count,1);
    return sent;
}
static void button(bsp_btn_t key,bsp_btn_ev_t event,void *user) {
    (void)user;
    if(event==BSP_BTN_PRESS || event==BSP_BTN_RELEASE)
        passport_app_post_input((passport_key_t)key,event==BSP_BTN_PRESS?
            PASSPORT_KEY_PRESS:PASSPORT_KEY_RELEASE,(uint64_t)esp_timer_get_time()/1000);
}
static void publish(passport_status_t status) {
    update_t update={.record=core_record,.status=status,.time_valid=time_valid};
    xQueueOverwrite(updates,&update);
}
static passport_status_t save_candidate(passport_record_t *candidate) {
    return storage_ready?passport_store_save(&backend,&core_record,candidate):PASSPORT_ERR_STORAGE;
}
static void core_step(const job_t *job) {
    passport_status_t status=PASSPORT_OK;
    if(atomic_load(&clock_synced)) time_valid=true;
    if(job->kind==0) {
        passport_record_t candidate=core_record;candidate.brightness=job->brightness;
        publish(save_candidate(&candidate));return;
    }
    /* Only this worker touches the slot until done is signalled, including after
     * a timed-out caller has returned. The slot contains no caller pointers. */
    const passport_request_t *request=&owned_request;
    memset(&owned_reply,0,sizeof owned_reply);
    owned_reply.id=request->id;owned_reply.op=request->op;
    if(request->op==PASSPORT_OP_SET) {
        if(!passport_profile_glyphs_supported(&request->profile,passport_assets_has_glyph,NULL))
            status=PASSPORT_ERR_UNSUPPORTED;
        else {
            passport_record_t candidate=core_record;candidate.profile=request->profile;
            status=save_candidate(&candidate);
        }
        publish(status);
    } else if(request->op==PASSPORT_OP_TIME) {
#ifdef PASSPORT_HOST_TEST
        extern int passport_test_set_time(int64_t);
        int result=passport_test_set_time(request->epoch);
#else
        struct timeval now={.tv_sec=(time_t)request->epoch,.tv_usec=0};
        int result=settimeofday(&now,NULL);
#endif
        if(result) status=PASSPORT_ERR_STORAGE;
        else time_valid=true;
        publish(status);
    }
    owned_reply.status=status;owned_reply.record=core_record;
    owned_reply.time_valid=time_valid;owned_reply.epoch=(int64_t)time(NULL);
    owned_reply.glyphs=passport_asset_glyphs;owned_reply.glyph_count=passport_asset_glyph_count;
    owned_reply.version=PASSPORT_VERSION;
    xSemaphoreGive(done); /* Last shared-slot access, paired with service(). */
}
static void core_worker(void *user) {
    (void)user;
    while(!atomic_load(&running) && !atomic_load(&stopping)) vTaskDelay(pdMS_TO_TICKS(10));
    while(!atomic_load(&stopping)) {
        job_t job;
        if(xQueueReceive(core_queue,&job,pdMS_TO_TICKS(25))==pdTRUE) core_step(&job);
    }
    xSemaphoreGive(core_exited);vTaskSuspend(NULL);
}
static passport_status_t service(const passport_request_t *request,passport_reply_t *reply,void *user) {
    (void)user;
    if(!atomic_load(&running) || atomic_load(&stopping) ||
       xSemaphoreTake(service_mutex,pdMS_TO_TICKS(100))!=pdTRUE) return PASSPORT_ERR_STORAGE;
    passport_status_t status=PASSPORT_ERR_STORAGE;
    if(pending) {
        if(xSemaphoreTake(done,0)!=pdTRUE) goto finish;
        pending=false;
    }
    owned_request=*request;pending=true;
    job_t job={.kind=1};
    if(xQueueSend(core_queue,&job,0)!=pdTRUE) {pending=false;goto finish;}
    if(xSemaphoreTake(done,pdMS_TO_TICKS(2000))!=pdTRUE) goto finish;
    pending=false;*reply=owned_reply;status=reply->status;
finish:
    xSemaphoreGive(service_mutex);return status;
}
/* Wi-Fi configuration callbacks: submit through the same single-writer worker
 * the USB protocol uses, then wait for the result so the page can show it. */
static passport_status_t wifi_submit_profile(const passport_profile_t *profile) {
    if(!profile) return PASSPORT_ERR_INVALID;
    passport_request_t request={.op=PASSPORT_OP_SET,.profile=*profile};
    passport_reply_t reply;
    return service(&request,&reply,NULL);
}
static passport_status_t wifi_submit_time(int64_t epoch) {
    if(epoch<1577836800 || epoch>4102444799) return PASSPORT_ERR_INVALID;
    passport_request_t request={.op=PASSPORT_OP_TIME,.epoch=epoch};
    passport_reply_t reply;
    return service(&request,&reply,NULL);
}
static void config_event(passport_config_event_t event) {
    if(!atomic_load(&running) || atomic_load(&stopping)) return;
    if(!bsp_lvgl_lock(1000)) return;
    /* Always drop the cached media: if an upload could not be loaded into
     * RAM while the hotspot was holding memory, the retry after teardown
     * (heap returned) picks it up. */
    passport_avatar_flash_invalidate();
    passport_image_flash_invalidate();
    if(event==PASSPORT_CONFIG_MEDIA_CHANGED) {
        passport_ui_status(PASSPORT_TEXT_STATUS_SAVED);
        passport_ui_acknowledge();
    } else {
        passport_ui_configuration(NULL,NULL);
        passport_ui_status(event==PASSPORT_CONFIG_FAILED?"配置页面启动失败，请重试":"");
    }
    bsp_lvgl_unlock();
}
__attribute__((weak)) int passport_agent_network_address(char *out,size_t cap) {
    snprintf(out,cap,"192.168.4.1");return 0;
}
static void start_config(void) {
    if(passport_config_running()) return;
    char address[32];int shared=passport_agent_network_address(address,sizeof address);
    passport_config_api_t callbacks={.submit_profile=wifi_submit_profile,
        .submit_time=wifi_submit_time,.on_event=config_event,
        .snapshot=ui_record.profile,.shared_station=shared>0};
    snprintf(callbacks.station_address,sizeof callbacks.station_address,"%.15s",address);
    if(passport_config_start(&callbacks)!=ESP_OK) {
        if(bsp_lvgl_lock(100)) {passport_ui_status(PASSPORT_TEXT_STATUS_BUSY);bsp_lvgl_unlock();}
        return;
    }
    char ssid[16],line[64];
    passport_config_ssid(ssid,sizeof ssid);
    snprintf(line,sizeof line,"%s%s%s",shared>0?"http://":ssid,shared>0?"":"  ",address);
    if(bsp_lvgl_lock(100)) {
        if(shared>0) {
            char password[13];
            if(passport_config_password(password,sizeof password)) passport_ui_configuration(address,password);
        } else {passport_ui_status(line);passport_ui_hotspot(ssid);}
        bsp_lvgl_unlock();
    }
}
static void render(void) {
    passport_agent_set_visible(model.page==PASSPORT_PAGE_AGENT);
    atomic_store(&public_battery,cached_battery);
    atomic_store(&public_brightness,model.brightness);atomic_store(&public_page,(unsigned)model.page);
    bool agent_updated=passport_agent_snapshot(&agent_snapshot);
    if(!bsp_lvgl_lock(100)) return;
    if(agent_updated) passport_ui_agent_snapshot(&agent_snapshot);
    passport_ui_render(&model,&ui_record,cached_battery,
                       ui_time_valid,(int64_t)time(NULL));
    bsp_lvgl_unlock();
}
static void action(passport_action_t result) {
    if(result==PASSPORT_ACTION_SAVE_BRIGHTNESS) {
        job_t job={.kind=0,.brightness=model.brightness};
        if(xQueueSend(core_queue,&job,0)!=pdTRUE) {
            model.brightness=ui_record.brightness;
            if(bsp_lvgl_lock(100)) {passport_ui_status(PASSPORT_TEXT_STATUS_BUSY);bsp_lvgl_unlock();}
        }
        bsp_display_backlight(model.brightness);
    } else if(result==PASSPORT_ACTION_ENTER_CONFIG) {
        if(passport_agent_transport()) {
            model.page=PASSPORT_PAGE_MUSE_SETUP;model.setup_choice=0;model.setup_confirm=false;
        } else start_config();
    } else if(result==PASSPORT_ACTION_ENTER_SHARED_CONFIG) start_config();
    else if(result==PASSPORT_ACTION_MUSE_REPAIR) (void)passport_agent_action(PASSPORT_AGENT_REPAIR,NULL);
    else if(result==PASSPORT_ACTION_AGENT_TOGGLE || result==PASSPORT_ACTION_AGENT_LEAVE || result==PASSPORT_ACTION_SAVE_NOTE) {
        unsigned command=result==PASSPORT_ACTION_AGENT_TOGGLE?PASSPORT_AGENT_TOGGLE:
            result==PASSPORT_ACTION_AGENT_LEAVE?PASSPORT_AGENT_LEAVE:PASSPORT_AGENT_SAVE_RESULT;
        bool accepted=result==PASSPORT_ACTION_AGENT_TOGGLE && pressed_confirmation?
            passport_agent_confirm_pairing(pressed_confirmation):passport_agent_action(command,NULL);
        if(!accepted && bsp_lvgl_lock(100)) {
            passport_ui_status(PASSPORT_TEXT_STATUS_BUSY);bsp_lvgl_unlock();
        }
    } else if(result==PASSPORT_ACTION_SCROLL_UP || result==PASSPORT_ACTION_SCROLL_DOWN) {
        if(bsp_lvgl_lock(100)) {
            passport_ui_agent_scroll(result==PASSPORT_ACTION_SCROLL_DOWN?1:-1);bsp_lvgl_unlock();
        }
    }
}
static void reset_input_until_release(void) {
    pressed_confirmation=0;
    (void)passport_agent_action(PASSPORT_AGENT_LEAVE,NULL);
    passport_model_reset_input(&model);xQueueReset(input_queue);wait_release=true;
    passport_ui_game_reset_input();game_input_active=false;
    bsp_button_set_combo_enabled(false);
}
static void input_step(uint64_t now) {
    if(!atomic_load(&input_ready)) {
        if(game_input_active) passport_ui_game_reset_input();
        game_input_active=false;bsp_button_set_combo_enabled(false);
        return; /* shutdown may be waiting for configuration or USB workers */
    }
    if(atomic_exchange(&input_lost,false)) reset_input_until_release();
    input_t input;
    while(xQueueReceive(input_queue,&input,0)==pdTRUE) {
        if(input.control==1) {
            model.brightness=(uint8_t)input.value;action(PASSPORT_ACTION_SAVE_BRIGHTNESS);continue;
        }
        if(input.control==2) {
            bool old_agent=model.page==PASSPORT_PAGE_AGENT;
            model.page=(passport_page_t)input.value;
            if(old_agent && model.page!=PASSPORT_PAGE_AGENT) (void)passport_agent_action(PASSPORT_AGENT_LEAVE,NULL);
            reset_input_until_release();break;
        }
        if(wait_release) {
            continue;
        }
        if(input.key==PASSPORT_KEY_OK && input.event==PASSPORT_KEY_PRESS)
            pressed_confirmation=model.page==PASSPORT_PAGE_AGENT && agent_snapshot.state==PASSPORT_AGENT_CONFIRM?
                agent_snapshot.confirmation:0;
        bool was_game=model.page==PASSPORT_PAGE_GAME;
        action(passport_model_input(&model,input.key,input.event,input.now));
        if(was_game && model.page!=PASSPORT_PAGE_GAME) {
            reset_input_until_release();break;
        }
        /* The game reads raw edges for movement/jump; the model still owns the
         * long-OK home shortcut. Game state lives outside LVGL, no lock. */
        if(model.page==PASSPORT_PAGE_GAME) passport_ui_game_key(input.key,input.event);
    }
    if(!wait_release) action(passport_model_tick(&model,now));
    if(game_input_active && model.page!=PASSPORT_PAGE_GAME) reset_input_until_release();
    if(wait_release && bsp_button_read_mv()>=BSP_BTN_MV_RELEASED_MIN) wait_release=false;
    bool game_now=model.page==PASSPORT_PAGE_GAME && !wait_release &&
        atomic_load(&input_ready) && !atomic_load(&stopping);
    if(game_input_active && !game_now) passport_ui_game_reset_input();
    game_input_active=game_now;
    bsp_button_set_combo_enabled(game_now);
}
static void ui_worker(void *user) {
    (void)user;
    while(!atomic_load(&running) && !atomic_load(&stopping)) vTaskDelay(pdMS_TO_TICKS(10));
    uint64_t last_render=0,last_battery=0;
    while(!atomic_load(&stopping)) {
        uint64_t now=(uint64_t)esp_timer_get_time()/1000;
        input_step(now);
        if(battery_ready && (!last_battery || now-last_battery>=5000)) {
            cached_battery=bsp_battery_soc();last_battery=now;
        }
        update_t update;
        if(xQueueReceive(updates,&update,0)==pdTRUE) {
            ui_record=update.record;ui_time_valid=update.time_valid;
            model.brightness=ui_record.brightness;bsp_display_backlight(model.brightness);
            if(bsp_lvgl_lock(100)) {
                passport_ui_status(update.status==PASSPORT_OK?PASSPORT_TEXT_STATUS_SAVED:PASSPORT_TEXT_STATUS_STORAGE_FAILED);
                if(update.status==PASSPORT_OK) passport_ui_acknowledge();
                bsp_lvgl_unlock();
            }
        }
        if(atomic_load(&clock_synced)) ui_time_valid=true;
        /* Render compares model/profile and changes only the header if unchanged. */
        if(now-last_render>=50) {render();last_render=now;}
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    xSemaphoreGive(ui_exited);vTaskSuspend(NULL);
}
static void release_resources(void) {
    if(input_queue) vQueueDelete(input_queue);
    if(core_queue) vQueueDelete(core_queue);
    if(updates) vQueueDelete(updates);
    input_queue=core_queue=updates=NULL;
    if(core_exited) vSemaphoreDelete(core_exited);
    if(ui_exited) vSemaphoreDelete(ui_exited);
    if(done) vSemaphoreDelete(done);
    if(service_mutex) vSemaphoreDelete(service_mutex);
    core_exited=ui_exited=done=service_mutex=NULL;
    passport_nvs_close();storage_ready=false;pending=false;
}
esp_err_t passport_app_start(void) {
    if(atomic_load(&running)) return ESP_OK;
    if(core_task || ui_task || ui_created) return ESP_ERR_INVALID_STATE;
    atomic_store(&stopping,false);atomic_store(&input_ready,false);atomic_store(&input_lost,false);
    pressed_confirmation=0;
    atomic_store(&public_battery,-1);atomic_store(&public_brightness,60);atomic_store(&public_page,PASSPORT_PAGE_BADGE);
    time_valid=ui_time_valid=false;pending=false;game_input_active=false;startup_status=NULL;cached_battery=-1;
    input_queue=xQueueCreate(16,sizeof(input_t));core_queue=xQueueCreate(4,sizeof(job_t));
    updates=xQueueCreate(1,sizeof(update_t));
    core_exited=xSemaphoreCreateBinary();ui_exited=xSemaphoreCreateBinary();
    done=xSemaphoreCreateBinary();service_mutex=xSemaphoreCreateMutex();
    if(!input_queue || !core_queue || !updates || !core_exited || !ui_exited || !done ||
       !service_mutex)
        goto allocation_failed;
    memset(&core_record,0,sizeof core_record);core_record.schema=1;core_record.brightness=60;
    storage_ready=nvs_flash_init()==ESP_OK && passport_nvs_open(&backend)==ESP_OK;
    if(storage_ready) {
        passport_status_t status=passport_store_load(&backend,&core_record);
        if(status!=PASSPORT_OK && status!=PASSPORT_ERR_NOT_FOUND) startup_status=PASSPORT_TEXT_STATUS_STORAGE_UNAVAILABLE;
    } else startup_status=PASSPORT_TEXT_STATUS_STORAGE_UNAVAILABLE;
    ui_record=core_record;passport_model_init(&model,core_record.brightness);
    if(bsp_display_init()!=ESP_OK || !bsp_lvgl_init() || !bsp_lvgl_lock(1000)) goto startup_failed;
    ui_created=passport_ui_create();
    if(ui_created) {
        if(startup_status) passport_ui_status(startup_status);
        passport_ui_render(&model,&ui_record,-1,false,0);
    }
    bsp_lvgl_unlock();
    if(!ui_created) goto startup_failed;
    bsp_display_backlight(model.brightness);
    battery_ready=bsp_battery_init()==ESP_OK;
    /* Measured peaks: store ~1.4 KB, UI ~2.1 KB; the Muse session needs the rest. */
    if(xTaskCreate(core_worker,"passport_store",4096,NULL,3,&core_task)!=pdPASS ||
       xTaskCreate(ui_worker,"passport_app",5120,NULL,4,&ui_task)!=pdPASS) goto startup_failed;
    if(bsp_button_init(button,NULL)!=ESP_OK) goto startup_failed;
    /* A failed ADC reading is treated as held until an observed release. */
    wait_release=bsp_button_read_mv()<BSP_BTN_MV_RELEASED_MIN;
    atomic_store(&running,true);atomic_store(&input_ready,true);
    if(passport_agent_start(passport_agent_transport())!=ESP_OK && bsp_lvgl_lock(100)) {
        passport_ui_status(PASSPORT_TEXT_STATUS_BUSY);bsp_lvgl_unlock();
    }
    if(passport_usb_start(service,NULL)!=ESP_OK) {
        if(bsp_lvgl_lock(100)) {passport_ui_status(PASSPORT_TEXT_STATUS_USB_UNAVAILABLE);bsp_lvgl_unlock();}
    }
    ESP_LOGI("passport","heap_free=%u largest_block=%u",
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    return ESP_OK;
startup_failed:
    /* Workers have not passed the running gate, so own no UI/backend resources. */
    if(core_task) {vTaskDelete(core_task);core_task=NULL;}
    if(ui_task) {vTaskDelete(ui_task);ui_task=NULL;}
    if(ui_created && bsp_lvgl_lock(1000)) {passport_ui_destroy();ui_created=false;bsp_lvgl_unlock();}
    release_resources();return ESP_FAIL;
allocation_failed:
    release_resources();return ESP_ERR_NO_MEM;
}
esp_err_t passport_app_stop(void) {
    if((ui_task && xTaskGetCurrentTaskHandle()==ui_task) || (core_task && xTaskGetCurrentTaskHandle()==core_task))
        return ESP_ERR_INVALID_STATE;
    atomic_store(&input_ready,false);
    esp_err_t agent_error=passport_agent_stop();
    if(agent_error!=ESP_OK) return agent_error;
    bsp_button_set_combo_enabled(false);
    esp_err_t config_error=passport_config_stop();
    if(config_error!=ESP_OK) return config_error;
    for(unsigned i=0;atomic_load(&callback_count) && i<100;i++) vTaskDelay(pdMS_TO_TICKS(1));
    if(atomic_load(&callback_count)) return ESP_ERR_TIMEOUT;
    esp_err_t error=passport_usb_stop();
    if(error!=ESP_OK) return error;
    atomic_store(&stopping,true);atomic_store(&running,false);
    if(core_task) {
        if(xSemaphoreTake(core_exited,pdMS_TO_TICKS(2000))!=pdTRUE) return ESP_ERR_TIMEOUT;
        vTaskDelete(core_task);core_task=NULL;
    }
    if(ui_task) {
        if(xSemaphoreTake(ui_exited,pdMS_TO_TICKS(2000))!=pdTRUE) return ESP_ERR_TIMEOUT;
        vTaskDelete(ui_task);ui_task=NULL;
    }
    bsp_button_set_combo_enabled(false); /* the UI owner can no longer re-enable it */
    if(ui_created) {
        if(!bsp_lvgl_lock(1000)) return ESP_ERR_TIMEOUT;
        passport_ui_destroy();ui_created=false;bsp_lvgl_unlock();
    }
    release_resources();return ESP_OK;
}
