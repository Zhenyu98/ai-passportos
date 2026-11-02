/* Compile the real adapter against bounded deterministic FreeRTOS/BSP fakes. */
#include "../main/passport_app.c"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
void passport_ui_acknowledge(void) {}
static unsigned agent_actions,agent_last_action;
static unsigned physical_confirmations;
const passport_agent_transport_t *passport_agent_transport(void) {return NULL;}
esp_err_t passport_agent_start(const passport_agent_transport_t *t) {(void)t;return ESP_OK;}
esp_err_t passport_agent_stop(void) {return ESP_OK;}
bool passport_agent_action(unsigned a,const char *t) {(void)t;agent_actions++;agent_last_action=a;return true;}
bool passport_agent_confirm_pairing(uint32_t request) {assert(request==42);physical_confirmations++;return true;}
bool passport_agent_snapshot(passport_agent_model_t *v) {(void)v;return false;}
void passport_agent_set_visible(bool value) {(void)value;}
void passport_ui_agent_snapshot(const passport_agent_model_t *v) {(void)v;}
void passport_ui_agent_scroll(int direction) {(void)direction;}
void passport_ui_configuration(const char *address,const char *password) {(void)address;(void)password;}
bool passport_config_password(char *out,size_t size) {(void)out;(void)size;return false;}

typedef struct {unsigned capacity,size,count,head;unsigned char *data;} fake_queue_t;
typedef struct {bool token;} fake_sem_t;
static unsigned queues,semaphores,tasks,task_calls,fail_task_at,queue_calls,fail_queue_at;
static bool fail_ui,fail_lock,fail_nvs,fail_battery,usb_started,config_started;
static int button_mv=3300;
static passport_page_t rendered_page;
static bsp_btn_cb_t callback;
static bool combo_enabled;
static unsigned game_input_resets, game_edges;
static bool schedule_input_during_stop;
static bool check_remote_guard;
static int last_backlight;
static int64_t monotonic_us;
int passport_test_set_time(int64_t epoch) {(void)epoch;return 0;}
bool passport_profile_glyphs_supported(const passport_profile_t *p,passport_glyph_fn fn,void *user) {(void)p;(void)fn;(void)user;return true;}
QueueHandle_t xQueueCreate(UBaseType_t n,UBaseType_t size) {
    if(++queue_calls==fail_queue_at)return NULL;
    fake_queue_t *q=calloc(1,sizeof *q);assert(q);q->capacity=n;q->size=size;q->data=calloc(n,size);assert(q->data);queues++;return q;
}
BaseType_t xQueueSend(QueueHandle_t handle,const void *data,TickType_t timeout) {
    if(check_remote_guard && handle==input_queue) assert(atomic_load(&callback_count)>0);
    (void)timeout;fake_queue_t *q=handle;if(q->count==q->capacity)return pdFALSE;
    memcpy(q->data+((q->head+q->count)%q->capacity)*q->size,data,q->size);q->count++;return pdTRUE;
}
BaseType_t xQueueReceive(QueueHandle_t handle,void *data,TickType_t timeout) {
    (void)timeout;fake_queue_t *q=handle;if(!q->count)return pdFALSE;
    memcpy(data,q->data+q->head*q->size,q->size);q->head=(q->head+1)%q->capacity;q->count--;return pdTRUE;
}
BaseType_t xQueueOverwrite(QueueHandle_t handle,const void *data) {
    fake_queue_t *q=handle;assert(q->capacity==1);q->count=0;return xQueueSend(q,data,0);
}
BaseType_t xQueueReset(QueueHandle_t handle) {fake_queue_t *q=handle;q->head=q->count=0;return pdTRUE;}
void vQueueDelete(QueueHandle_t handle) {fake_queue_t *q=handle;free(q->data);free(q);queues--;}
SemaphoreHandle_t xSemaphoreCreateBinary(void) {semaphores++;return calloc(1,sizeof(fake_sem_t));}
SemaphoreHandle_t xSemaphoreCreateMutex(void) {fake_sem_t *s=xSemaphoreCreateBinary();s->token=true;return s;}
BaseType_t xSemaphoreTake(SemaphoreHandle_t handle,TickType_t timeout) {
    (void)timeout;fake_sem_t *s=handle;if(!s->token)return pdFALSE;s->token=false;return pdTRUE;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t handle) {((fake_sem_t *)handle)->token=true;return pdTRUE;}
void vSemaphoreDelete(SemaphoreHandle_t handle) {free(handle);semaphores--;}
BaseType_t xTaskCreate(TaskFunction_t fn,const char *name,uint32_t stack,void *arg,UBaseType_t priority,TaskHandle_t *out) {
    (void)fn;(void)name;(void)stack;(void)arg;(void)priority;
    if(++task_calls==fail_task_at)return pdFALSE;
    *out=malloc(1);tasks++;return pdPASS;
}
void vTaskDelete(TaskHandle_t handle) {assert(handle);free(handle);tasks--;}
void vTaskDelay(TickType_t ticks) {monotonic_us+=(int64_t)ticks*1000;}
void vTaskSuspend(TaskHandle_t handle) {(void)handle;}
TaskHandle_t xTaskGetCurrentTaskHandle(void) {return NULL;}
int64_t esp_timer_get_time(void) {return monotonic_us;}
esp_err_t nvs_flash_init(void) {return fail_nvs?ESP_FAIL:ESP_OK;}
esp_err_t passport_nvs_open(passport_store_backend_t *b) {(void)b;return ESP_FAIL;}
void passport_nvs_close(void) {}
esp_err_t bsp_display_init(void) {return ESP_OK;}
struct _lv_display_t *bsp_lvgl_init(void) {return (void *)1;}
bool bsp_lvgl_lock(int ms) {(void)ms;return !fail_lock;}
void bsp_lvgl_unlock(void) {}
void bsp_display_backlight(uint8_t value) {last_backlight=value;}
esp_err_t bsp_button_init(bsp_btn_cb_t cb,void *user) {(void)user;callback=cb;return ESP_OK;}
void bsp_button_set_combo_enabled(bool enabled) {combo_enabled=enabled;}
int bsp_button_read_mv(void) {return button_mv;}
esp_err_t bsp_battery_init(void) {return fail_battery?ESP_FAIL:ESP_OK;}
int bsp_battery_soc(void) {return fail_battery?-1:80;}
bool passport_ui_create(void) {return !fail_ui;}
bool passport_ui_render(const passport_model_t *m,const passport_record_t *r,int battery,bool valid,int64_t epoch) {
    (void)r;(void)battery;(void)valid;(void)epoch;rendered_page=m->page;return true;
}
void passport_ui_destroy(void) {}
void passport_ui_status(const char *status) {(void)status;}
void passport_ui_game_key(passport_key_t key,passport_key_event_t event) {(void)key;(void)event;game_edges++;}
void passport_ui_game_reset_input(void) {game_input_resets++;}
bool passport_assets_has_glyph(uint32_t cp,void *user) {(void)cp;(void)user;return true;}
const uint32_t passport_asset_glyphs[]={65};
const size_t passport_asset_glyph_count=1;
esp_err_t passport_usb_start(passport_service_fn fn,void *arg) {(void)fn;(void)arg;usb_started=true;return ESP_OK;}
esp_err_t passport_usb_stop(void) {
    if(schedule_input_during_stop) {
        input_step((uint64_t)monotonic_us/1000);
        assert(!combo_enabled); /* UI interleaves while shutdown is in progress */
    }
    usb_started=false;return ESP_OK;
}
esp_err_t passport_config_start(const passport_config_api_t *api) {
    (void)api;config_started=true;return ESP_OK;
}
esp_err_t passport_config_stop(void) {config_started=false;return ESP_OK;}
bool passport_config_running(void) {return config_started;}
void passport_config_ssid(char *buffer,size_t size) {snprintf(buffer,size,"Badge-TEST");}
void passport_avatar_flash_invalidate(void) {}
void passport_image_flash_invalidate(void) {}
static void stop_ready(void) { xSemaphoreGive(core_exited);xSemaphoreGive(ui_exited);assert(passport_app_stop()==ESP_OK);assert(!queues&&!semaphores&&!tasks&&!usb_started); }
int main(void) {
    for(unsigned i=1;i<=3;i++) {
        queue_calls=0;fail_queue_at=i;
        assert(passport_app_start()==ESP_ERR_NO_MEM);assert(!queues&&!semaphores&&!tasks);
    }
    fail_queue_at=0;fail_ui=true;
    assert(passport_app_start()!=ESP_OK);assert(!queues&&!semaphores&&!tasks);fail_ui=false;
    for(unsigned i=1;i<=2;i++) {
        task_calls=0;fail_task_at=i;
        /* First worker is initially gated and may be deleted before ownership begins. */
        assert(passport_app_start()!=ESP_OK);assert(!queues&&!semaphores&&!tasks);
    }
    fail_task_at=0;fail_nvs=fail_battery=true;button_mv=100;
    assert(passport_app_start()==ESP_OK);
    assert(callback);
    callback(BSP_BTN_OK,BSP_BTN_LONG,NULL); /* BSP 500 ms LONG must be ignored. */
    assert(model.page==PASSPORT_PAGE_BADGE && rendered_page==PASSPORT_PAGE_BADGE && last_backlight==60);
    assert(wait_release);
    assert(passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS,10));
    input_step(10);assert(model.held_key==-1);
    button_mv=3300;
    input_step(20);assert(!wait_release);
    assert(passport_app_post_input(PASSPORT_KEY_DOWN,PASSPORT_KEY_PRESS,30));
    assert(passport_app_post_input(PASSPORT_KEY_DOWN,PASSPORT_KEY_RELEASE,40));
    input_step(40);assert(model.page==PASSPORT_PAGE_BADGE && model.badge_button==1);
    assert(!combo_enabled);
    check_remote_guard=true;
    assert(passport_app_remote_brightness(60));
    input_step(40);
    check_remote_guard=false;
    xQueueReset(core_queue);
    model.page=PASSPORT_PAGE_AGENT;
    agent_snapshot.state=PASSPORT_AGENT_READY;
    passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS,41);input_step(41);
    agent_snapshot.state=PASSPORT_AGENT_CONFIRM;agent_snapshot.confirmation=42;
    passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_RELEASE,42);input_step(42);
    assert(!physical_confirmations); /* Prompt arriving mid-click is not consent. */
    passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS,43);input_step(43);
    passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_RELEASE,44);input_step(44);
    assert(physical_confirmations==1);
    pressed_confirmation=0;
    action(PASSPORT_ACTION_AGENT_TOGGLE);
    assert(agent_actions && agent_last_action==PASSPORT_AGENT_TOGGLE);
    assert(passport_app_remote_page(PASSPORT_PAGE_BADGE));
    input_step(40);
    assert(model.page==PASSPORT_PAGE_BADGE && agent_last_action==PASSPORT_AGENT_LEAVE);
    model.page=PASSPORT_PAGE_GAME;
    input_step(41); assert(combo_enabled);
    unsigned edges_before=game_edges;
    callback(BSP_BTN_DOWN,BSP_BTN_PRESS,NULL);
    callback(BSP_BTN_OK,BSP_BTN_PRESS,NULL);
    input_step(42); assert(game_edges==edges_before+2);
    button_mv=212;
    input_step(2042); /* hold the actual chord for the home shortcut */
    assert(model.page==PASSPORT_PAGE_BADGE && !combo_enabled && game_input_resets>0);
    button_mv=212;
    monotonic_us=2043*1000;
    callback(BSP_BTN_OK,BSP_BTN_RELEASE,NULL); /* decoder changes to single-key */
    input_step(2043);
    button_mv=595;
    monotonic_us=2044*1000;
    callback(BSP_BTN_OK,BSP_BTN_PRESS,NULL); /* DOWN released, OK still held */
    input_step(2044);
    button_mv=3300;
    monotonic_us=2045*1000;
    callback(BSP_BTN_OK,BSP_BTN_RELEASE,NULL);
    input_step(2045);
    assert(model.page==PASSPORT_PAGE_BADGE && model.badge_button==1);
    passport_model_reset_input(&model);
    model.page=PASSPORT_PAGE_GAME;
    input_step(44); assert(combo_enabled);
    unsigned resets_before=game_input_resets;
    button_mv=212;
    for(unsigned i=0;i<32;i++) passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS,50);
    assert(atomic_load(&input_lost));
    input_step(50);assert(model.held_key==-1 && wait_release);
    assert(game_input_resets>resets_before && !combo_enabled);
    passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_RELEASE,51);
    input_step(51);assert(wait_release); /* DOWN is still physically held */
    button_mv=3300;
    input_step(52);assert(!wait_release && combo_enabled);
    /* A timed out USB request retains its owned slot until the worker acknowledges. */
    passport_request_t request={.id=9,.op=PASSPORT_OP_GET};
    passport_reply_t reply;
    assert(service(&request,&reply,NULL)==PASSPORT_ERR_STORAGE && pending);
    unsigned pending_jobs=((fake_queue_t *)core_queue)->count;
    passport_profile_t next_profile={0};
    storage_ready=true;
    assert(wifi_submit_profile(&next_profile)==PASSPORT_ERR_STORAGE);
    assert(((fake_queue_t *)core_queue)->count==pending_jobs);
    storage_ready=false;
    assert(service(&request,&reply,NULL)==PASSPORT_ERR_STORAGE && pending);
    job_t job;assert(xQueueReceive(core_queue,&job,0));core_step(&job);
    assert(xSemaphoreTake(done,0));xSemaphoreGive(done);
    assert(service(&request,&reply,NULL)==PASSPORT_ERR_STORAGE && pending);
    assert(xQueueReceive(core_queue,&job,0));core_step(&job);
    /* No failure path touches a caller's expired request/reply. */
    assert(!storage_ready);
    job=(job_t){.kind=0,.brightness=90};core_step(&job);
    update_t update;assert(xQueueReceive(updates,&update,0));
    assert(update.status==PASSPORT_ERR_STORAGE && update.record.brightness==60);
    /* Network time is sticky and reaches USB replies through the store owner. */
    assert(!update.time_valid && !time_valid);
    passport_app_clock_synced();core_step(&job);
    assert(time_valid && xQueueReceive(updates,&update,0) && update.time_valid);
    unsigned live_queues=queues,live_semaphores=semaphores,live_tasks=tasks;
    schedule_input_during_stop=true;
    assert(passport_app_stop()==ESP_ERR_TIMEOUT);
    assert(queues==live_queues && semaphores==live_semaphores && tasks==live_tasks);
    stop_ready();
    assert(!combo_enabled);
    assert(passport_app_stop()==ESP_OK);
    assert(!passport_app_post_input(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS,100));
    puts("Passport runtime ownership, allocation failure and input recovery: PASS");
    return 0;
}
