#include "passport_agent.h"
#include "passport_app.h"
#include "muse_board.h"
#include "muse_chat.h"
#include "muse_link.h"
#include "muse_settings.h"
#include "muse_state.h"
#include "muse_wifi.h"
#include "muse_ble.h"
#include "sdk_token_console.h"
#include "identity.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include <stdio.h>
#include <string.h>
#include <sys/time.h>

/* The official boot glue calls this entry after NVS/identity are ready. */
const muse_board_t *muse_board;
void passport_muse_reply(char *,size_t);
uint32_t passport_muse_pending_session(void);
bool passport_muse_confirm_pairing(uint32_t);
static void setting(muse_setting_t what) {
    if(what==MUSE_SETTING_WIFI) muse_wifi_apply();
    else if(what==MUSE_SETTING_BLE) muse_ble_apply();
}
void muse_app_run(const muse_board_t *board) {
    muse_board=board; /* The SDK BSP never initializes hardware; Passport owns it. */
    esp_err_t err=muse_settings_init();
    if(err!=ESP_OK) {ESP_LOGE("passport","Muse settings unavailable: %s",esp_err_to_name(err));return;}
    muse_settings_set_listener(setting);muse_state_init();muse_hatch_start();
    err=passport_app_start();
    if(err!=ESP_OK) ESP_LOGE("passport","Application startup failed: %s",esp_err_to_name(err));
}
static void clock_synced(struct timeval *now) {(void)now;passport_app_clock_synced();}
/* The SDK creates esp_netif after Muse starts, so wait for the first address
 * before starting SNTP. lwIP then retries and renews (hourly) on its own;
 * the callback runs in the lwIP task and only sets an atomic flag. */
static void start_clock(void) {
    static bool started;
    if(started) return;
    muse_wifi_status_t status;muse_wifi_status(&status);
    if(status.state!=MUSE_WIFI_CONNECTED || !status.ip[0]) return;
    esp_sntp_config_t config=ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(3,
        ESP_SNTP_SERVER_LIST("ntp.aliyun.com","cn.pool.ntp.org","pool.ntp.org"));
    config.wait_for_sync=false;config.sync_cb=clock_synced;
    esp_err_t err=esp_netif_sntp_init(&config);
    started=err==ESP_OK;
    if(!started) ESP_LOGW("passport","SNTP start failed: %s",esp_err_to_name(err));
}
static passport_agent_state_t connection(void) {
    start_clock(); /* polled by the agent worker on every page */
    int battery;passport_app_device_status(&battery,NULL,NULL);
    muse_power_t power={.battery_pct=battery};muse_state_set_power(&power);
    switch(muse_link_state()) {
        case MUSE_LINK_UNPAIRED:case MUSE_LINK_PAIRING:return PASSPORT_AGENT_UNPAIRED;
        case MUSE_LINK_CONFIRM:return PASSPORT_AGENT_CONFIRM;
        case MUSE_LINK_ONLINE:return muse_hatch_ready()?PASSPORT_AGENT_READY:PASSPORT_AGENT_CONNECTING;
        case MUSE_LINK_OFFLINE:case MUSE_LINK_ERROR:return PASSPORT_AGENT_OFFLINE;
        default:return PASSPORT_AGENT_CONNECTING;
    }
}
static bool begin(void) {
    if(!muse_hatch_ready()) return false;
    muse_hatch_turn_begin();return true;
}
static int event(char *out,size_t cap) {
    muse_hatch_ev_t ev=muse_hatch_turn_event(out,cap);
    if(ev==MUSE_HATCH_EV_HEARD) return 1;
    if(ev==MUSE_HATCH_EV_REPLY || ev==MUSE_HATCH_EV_DONE) {
        passport_muse_reply(out,cap);return ev==MUSE_HATCH_EV_DONE?3:2;
    }
    return ev==MUSE_HATCH_EV_ERROR?4:0;
}
static void describe(char *out,size_t cap) {
    muse_wifi_status_t status;muse_wifi_status(&status);
    char ssid[33];if(!passport_agent_copy(ssid,sizeof ssid,status.ssid)) strcpy(ssid,"(SSID)");
    const char *token=muse_link_sdk_token();
    snprintf(out,cap,"%.24s\nWi-Fi: %.32s\nIP: %.15s\nSDK Token: %s",identity_ble_name(),ssid,status.ip,
        token && *token?"OK":"USB setup required");
}
const passport_agent_transport_t *passport_agent_transport(void) {
    static const passport_agent_transport_t ops={
        .connection=connection,.confirm=passport_muse_confirm_pairing,.begin=begin,
        .audio=muse_hatch_turn_audio,.end=muse_hatch_turn_end,.cancel=muse_hatch_turn_cancel,.event=event,
        .confirmation=passport_muse_pending_session,.repair=muse_link_reset_setup,.describe=describe};return &ops;
}
bool passport_usb_external_line(const char *line,size_t length) {
    return length>1 && line[0]=='>' && sdk_token_console_command(line+1);
}
int passport_agent_network_address(char *out,size_t cap) {
    muse_wifi_status_t status;muse_wifi_status(&status);
    if(status.state!=MUSE_WIFI_CONNECTED || !status.ip[0]) return -1;
    snprintf(out,cap,"%s",status.ip);return 1;
}
