/* SoftAP configuration service: Wi-Fi hotspot + embedded mobile web page. */
#include "passport_config.h"
#include "passport_config_auth.h"
#include "passport_assets.h"
#include "passport_avatar_flash.h"
#include "passport_protocol.h"
#include "passport_settings.h"
#include <stdlib.h>
#include "cJSON.h"
#include "esp_event.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_random.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "lwip/sockets.h"
#include <stdio.h>
#include <string.h>

static passport_config_auth_t session_auth;
static char session_password[13];
static bool authorized(httpd_req_t *req) {
    if(!session_auth.required) return true;
    char host[48]={0},origin[96]={0},credentials[80]={0};
    if(httpd_req_get_hdr_value_str(req,"Host",host,sizeof host)!=ESP_OK) host[0]=0;
    if(httpd_req_get_hdr_value_str(req,"Origin",origin,sizeof origin)!=ESP_OK &&
       httpd_req_get_hdr_value_len(req,"Origin")>0) strcpy(origin,"invalid");
    if(httpd_req_get_hdr_value_str(req,"Authorization",credentials,sizeof credentials)!=ESP_OK) credentials[0]=0;
    httpd_resp_set_hdr(req,"Cache-Control","no-store");
    if(passport_config_auth_accept(&session_auth,host,origin,credentials)) return true;
    httpd_resp_set_status(req,"401 Unauthorized");
    httpd_resp_set_hdr(req,"WWW-Authenticate","Basic realm=\"Passport\", charset=\"UTF-8\"");
    httpd_resp_sendstr(req,"Use the temporary access code shown on the badge. Username: passport.");
    return false;
}
static esp_err_t authorized_dispatch(httpd_req_t *req) {
    if(!authorized(req)) return ESP_OK;
    const httpd_uri_t *route=req->user_ctx;
    return route->handler(req);
}

#define CONFIG_TIMEOUT_MS 300000 /* Five minutes, then the hotspot closes. */
#define BODY_MAX 8192            /* Bounded heap buffer, not a task-stack array. */
#define CHUNK_MAX 4096           /* Media upload streaming chunk. */

static passport_config_api_t owned_api;
static const passport_config_api_t *api=&owned_api;
static passport_profile_t shown;
static TaskHandle_t task_handle;
static SemaphoreHandle_t stop_signal, exit_signal;
static httpd_handle_t server;
static esp_netif_t *netif;
static bool active;
static volatile bool dns_running;

static const char PAGE[] =
"<!doctype html><html lang='zh-CN'><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>工牌配置</title><style>"
"body{font-family:sans-serif;background:#0a1c33;color:#f4faff;margin:0;padding:16px}"
"h1{font-size:18px;color:#3dff9e;margin:0 0 2px}.sub{color:#8fa3c0;font-size:12px;margin-bottom:14px}"
"form{background:#0c1d3a;border:1px solid #1a4a6e;border-radius:12px;padding:14px;margin-bottom:14px}"
"label{display:block;font-size:12px;color:#8fa3c0;margin:10px 0 3px}"
"input[type=text],textarea{width:100%;box-sizing:border-box;background:#081428;"
"border:1px solid #1a4a6e;border-radius:6px;color:#f4faff;padding:8px;font-size:14px}"
"button{margin-top:12px;width:100%;padding:11px;font-size:14px;border-radius:8px;"
"border:2px solid #3dff9e;background:#0c1d3a;color:#3dff9e}"
".status{font-size:12px;margin-top:8px;min-height:16px}.err{color:#ff7a7a}.ok{color:#3dff9e}"
"canvas{image-rendering:pixelated;border:1px solid #1a4a6e;border-radius:8px;"
"background:#081428;display:block;margin-top:6px}"
"#cv2{width:240px;max-width:100%;touch-action:none}"
"input[type=range]{width:100%;margin:6px 0}"
"#grid{display:grid;grid-template-columns:1fr 1fr;gap:8px;margin-top:8px}"
"#grid button{margin-top:0;padding:9px 4px;font-size:13px;border-width:1px;color:#f4faff;"
"border-color:#1a4a6e;white-space:nowrap;overflow:hidden;text-overflow:ellipsis}"
"#grid button.on{border-color:#3dff9e;color:#3dff9e}"
"</style></head><body><h1>工牌配置</h1>"
"<div class='sub'>连接热点后自动弹出本页；五分钟无操作热点自动关闭。</div>"
"<form id='f'><label>姓名</label><input type='text' name='name' maxlength='64'>"
"<label>职位 / 身份</label><input type='text' name='title' maxlength='96'>"
"<label>单位 / 标签</label><input type='text' name='research' maxlength='192'>"
"<label>简介</label><textarea name='bio' rows='2' maxlength='512'></textarea>"
"<label>个人主页链接</label>"
"<input type='text' name='homepage' maxlength='128' placeholder='https://...'>"
"<p>保存资料不会写入背面 NFC。碰一碰分享需用手机 NFC 读写 App，另将同一网址写为 URL/URI 记录。</p>"
"<button type='submit'>保存资料</button><div class='status' id='s1'></div></form>"
"<form id='g'><label>像素头像场景</label><div id='grid'></div>"
"<div class='status' id='s3'></div></form>"
"<form id='i'><label>主页图片（整页裁剪 240×320，可拖动 / 缩放）</label>"
"<input type='file' id='pic2' accept='image/png,image/jpeg'>"
"<canvas id='cv2' width='240' height='320'></canvas>"
"<input type='range' id='z2' min='100' max='300' value='100'>"
"<button type='submit' id='up2'>上传主页图片</button>"
"<button type='button' id='rst2' style='border-color:#8fa3c0;color:#8fa3c0'>清除主页图片</button>"
"<div class='status' id='s4'></div></form>"
"<script>"
"function set(id,cls,text){const el=document.getElementById(id);el.className='status '+cls;el.textContent=text}"
"async function load(){try{const r=await fetch('/api/profile');const d=await r.json();"
"for(const k of ['name','title','research','bio','homepage']){const el=document.querySelector('[name='+k+']');"
"if(el)el.value=d[k]||'';}}catch(e){}}"
"load();"
"document.getElementById('f').addEventListener('submit',async e=>{e.preventDefault();"
"const b={};for(const i of e.target.querySelectorAll('input,textarea'))b[i.name]=i.value;"
"b.epoch_ms=Date.now();try{const r=await fetch('/api/profile',{method:'POST',"
"headers:{'Content-Type':'application/json'},body:JSON.stringify(b)});const d=await r.json();"
"d.ok?set('s1','ok',d.message||'已保存'):set('s1','err',d.error||'保存失败');}catch(x){set('s1','err','网络错误')}});"
"const grid=document.getElementById('grid');const saved=localStorage.getItem('scene');"
"fetch('/api/scenes').then(r=>r.json()).then(list=>{for(const s of list){"
"const b=document.createElement('button');b.type='button';b.textContent=s.n;"
"if(String(s.i)===saved)b.className='on';"
"b.onclick=async()=>{try{const r=await fetch(s.i<0?'/api/avatar/animated?i='+(-s.i):'/api/avatar/scene?i='+s.i,{method:'POST'});"
"const d=await r.json();if(d.ok){localStorage.setItem('scene',s.i);"
"for(const x of grid.children)x.className='';b.className='on';set('s3','ok','已切换：'+s.n);}"
"else set('s3','err',d.error||'切换失败');}catch(x){set('s3','err','网络错误')}};"
"grid.appendChild(b);}}).catch(()=>set('s3','err','场景列表加载失败'));"
"let iblob=null,img2=null,zf=1,offx=0,offy=0,drag=null;"
"const cv2=document.getElementById('cv2'),cx2=cv2.getContext('2d'),W=240,H=320;"
"function picScale(){return Math.max(W/img2.width,H/img2.height)*zf}"
"function clampPic(){const s=picScale(),w=img2.width*s,h=img2.height*s;"
"offx=Math.min(0,Math.max(W-w,offx));offy=Math.min(0,Math.max(H-h,offy));}"
"function drawPic(){if(!img2)return;clampPic();const s=picScale(),w=img2.width*s,h=img2.height*s;"
"cx2.fillStyle='#fff';cx2.fillRect(0,0,W,H);cx2.drawImage(img2,offx,offy,w,h);"
"const px=cx2.getImageData(0,0,W,H).data;iblob=new Uint8Array(W*H*2);"
"for(let i=0;i<W*H;i++){const v=((px[i*4]>>3)<<11)|((px[i*4+1]>>2)<<5)|(px[i*4+2]>>3);iblob[i*2]=v&255;iblob[i*2+1]=v>>8;}}"
"document.getElementById('pic2').addEventListener('change',e=>{const f=e.target.files[0];if(!f)return;"
"const img=new Image();img.onload=()=>{img2=img;zf=1;const s=Math.max(W/img.width,H/img.height);"
"offx=(W-img.width*s)/2;offy=(H-img.height*s)/2;drawPic();};img.src=URL.createObjectURL(f);});"
"document.getElementById('z2').addEventListener('input',e=>{if(!img2)return;const old=picScale();zf=(+e.target.value)/100;"
"const nx=(W/2-offx)/old,ny=(H/2-offy)/old;offx=W/2-nx*picScale();offy=H/2-ny*picScale();drawPic();});"
"cv2.addEventListener('pointerdown',e=>{drag={x:e.clientX,y:e.clientY,ox:offx,oy:offy};cv2.setPointerCapture(e.pointerId);});"
"cv2.addEventListener('pointermove',e=>{if(!drag||!img2)return;const k=W/cv2.clientWidth;"
"offx=drag.ox+(e.clientX-drag.x)*k;offy=drag.oy+(e.clientY-drag.y)*k;drawPic();});"
"cv2.addEventListener('pointerup',()=>{drag=null;});"
"document.getElementById('i').addEventListener('submit',async e=>{e.preventDefault();"
"if(!iblob){set('s4','err','先选择图片');return}"
"try{const r=await fetch('/api/image?w=240&h=320',{method:'POST',body:iblob});"
"const d=await r.json();d.ok?set('s4','ok','图片已更新，翻到主页页查看'):set('s4','err',d.error||'上传失败');}catch(x){set('s4','err','网络错误')}});"
"document.getElementById('rst2').addEventListener('click',async()=>{"
"try{await fetch('/api/image/reset',{method:'POST'});set('s4','ok','已清除主页图片');}catch(x){set('s4','err','网络错误')}});"
"</script></body></html>";

void passport_config_ssid(char *buffer, size_t size) {
    uint8_t mac[6];
    if (!buffer || size < 8) return;
    if (esp_read_mac(mac, ESP_MAC_WIFI_SOFTAP) != ESP_OK)
        snprintf(buffer, size, "Badge");
    else
        snprintf(buffer, size, "Badge-%02X%02X", mac[4], mac[5]);
}

static esp_err_t send_json(httpd_req_t *req, const char *body) {
    httpd_resp_set_type(req, "application/json");
    return httpd_resp_send(req, body, HTTPD_RESP_USE_STRLEN);
}

static esp_err_t get_index(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    return httpd_resp_send(req, PAGE, sizeof PAGE - 1);
}

/* Captive-portal probes redirect to the config page so joining the hotspot
 * pops the settings UI automatically on Android, iOS and Windows. */
static esp_err_t portal(httpd_req_t *req) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    return httpd_resp_send(req, NULL, 0);
}

/* Any unrecognized path is treated as a captive-portal probe and redirected to
 * the landing page, so unknown OS check URLs still trigger the popup. */
static esp_err_t not_found(httpd_req_t *req, httpd_err_code_t err) {
    (void)err;
    if(!authorized(req)) return ESP_OK;
    return portal(req);
}

/* Minimal DNS responder: every name resolves to 192.168.4.1 so OS
 * connectivity probes reach this device and trigger the portal popup. */
static void dns_task(void *user) {
    (void)user;
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) { vTaskDelete(NULL); return; }
    struct sockaddr_in address = {.sin_family = AF_INET, .sin_port = htons(53),
                                   .sin_addr.s_addr = htonl(INADDR_ANY)};
    if (bind(sock, (struct sockaddr *)&address, sizeof address) != 0) {
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    struct timeval timeout = {.tv_sec = 0, .tv_usec = 250000};
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);
    uint8_t query[512];
    while (dns_running) {
        struct sockaddr_in from;
        socklen_t from_len = sizeof from;
        int length = recvfrom(sock, query, sizeof query, 0, (struct sockaddr *)&from, &from_len);
        if (length < 12 || length > (int)(sizeof query - 16)) continue;
        /* Locate the question type: header + QNAME labels + QTYPE + QCLASS. */
        int q = 12;
        while (q < length && query[q] != 0) {
            if ((query[q] & 0xc0) == 0xc0) { q += 2; break; } /* compressed name */
            q += 1 + query[q];
        }
        if (q + 4 > length) continue;
        uint16_t qtype = (uint16_t)((query[q + 1] << 8) | query[q + 2]);
        ESP_LOGI("dns", "query type=%u len=%d", qtype, length);
        uint8_t response[528];
        memcpy(response, query, (size_t)length);
        response[2] = 0x81; response[3] = 0x80; /* QR|RD|RA, no error */
        int tail = length;
        if (qtype == 1) { /* A: answer with the portal address. */
            response[6] = 0; response[7] = 1;        /* one answer */
            response[8] = 0; response[9] = 0;        /* nscount 0 */
            response[10] = 0; response[11] = 0;      /* arcount 0 */
            response[tail++] = 0xc0; response[tail++] = 0x0c; /* name pointer */
            response[tail++] = 0; response[tail++] = 1;       /* type A */
            response[tail++] = 0; response[tail++] = 1;       /* class IN */
            response[tail++] = 0; response[tail++] = 0;
            response[tail++] = 0; response[tail++] = 60;      /* ttl */
            response[tail++] = 0; response[tail++] = 4;
            response[tail++] = 192; response[tail++] = 168; response[tail++] = 4; response[tail++] = 1;
        } else { /* AAAA and others: NODATA, so clients fall back to IPv4. */
            response[6] = 0; response[7] = 0;
            response[8] = 0; response[9] = 0;
            response[10] = 0; response[11] = 0;
        }
        (void)sendto(sock, response, (size_t)tail, 0, (struct sockaddr *)&from, from_len);
    }
    close(sock);
    vTaskDelete(NULL);
}

static esp_err_t get_profile(httpd_req_t *req) {
    cJSON *root = cJSON_CreateObject();
    const char *const names[5] = {"name", "title", "research", "bio", "homepage"};
    const char *const fields[5] = {shown.name, shown.title, shown.research, shown.bio, shown.homepage};
    bool ok = root != NULL;
    for (unsigned i = 0; ok && i < 5; ++i)
        ok = cJSON_AddStringToObject(root, names[i], fields[i]) != NULL;
    char *text = ok ? cJSON_PrintUnformatted(root) : NULL;
    esp_err_t result = text ? send_json(req, text) : send_json(req, "{\"ok\":false,\"error\":\"内存不足\"}");
    cJSON_free(text);
    cJSON_Delete(root);
    return result;
}

static size_t read_body(httpd_req_t *req, char *buffer, size_t capacity) {
    size_t total = 0;
    unsigned timeouts=0;
    while (total < capacity) {
        int got = httpd_req_recv(req, buffer + total, (size_t)(capacity - total));
        if (got == HTTPD_SOCK_ERR_TIMEOUT) {if(++timeouts>=3) return 0;continue;}
        if (got < 0) return 0;
        if (got == 0) break;
        total += (size_t)got;
    }
    buffer[total] = 0;
    return total;
}

static esp_err_t settings_failure(httpd_req_t *req,passport_status_t status,
                                  const passport_glyph_issue_t *issue) {
    char message[256];passport_settings_error(status,issue,message,sizeof message);
    cJSON *root=cJSON_CreateObject();
    bool ok=root && cJSON_AddBoolToObject(root,"ok",false)
        && cJSON_AddStringToObject(root,"code",passport_status_name(status))
        && cJSON_AddStringToObject(root,"error",message);
    char *json=ok?cJSON_PrintUnformatted(root):NULL;
    esp_err_t result=send_json(req,json?json:"{\"ok\":false,\"error\":\"设备内存不足，请稍后重试\"}");
    cJSON_free(json);cJSON_Delete(root);return result;
}
static esp_err_t post_profile(httpd_req_t *req) {
    if(req->content_len<=0 || req->content_len>BODY_MAX)
        return settings_failure(req,PASSPORT_ERR_CAPACITY,NULL);
    char *body=malloc((size_t)req->content_len+1);
    if(!body) return settings_failure(req,PASSPORT_ERR_STORAGE,NULL);
    size_t length=read_body(req,body,(size_t)req->content_len);
    passport_settings_update_t update;
    passport_status_t status=length==(size_t)req->content_len
        ?passport_settings_parse(body,length,&shown,&update):PASSPORT_ERR_INVALID;
    free(body);
    if(status!=PASSPORT_OK) return settings_failure(req,status,NULL);
    passport_glyph_issue_t issue;
    status=passport_profile_check(&update.profile,passport_assets_has_glyph,NULL,&issue);
    if(status!=PASSPORT_OK) return settings_failure(req,status,&issue);
    status=api->submit_profile(&update.profile);
    if(status!=PASSPORT_OK) return settings_failure(req,status,NULL);
    shown=update.profile;
    /* A rejected profile never changes the clock. A later clock failure must
     * not misreport an already committed profile as a failed save. */
    if(update.has_epoch && api->submit_time(update.epoch)!=PASSPORT_OK)
        return send_json(req,"{\"ok\":true,\"time_synced\":false,\"message\":\"资料已保存，校时暂未成功\"}");
    return send_json(req,"{\"ok\":true}");
}

static esp_err_t get_scenes(httpd_req_t *req) {
    cJSON *root = cJSON_CreateArray();
    bool ok = root != NULL;
    for(size_t i=0;ok && i<passport_motion_scene_count;i++) {
        const passport_motion_scene_t *scene=&passport_motion_scenes[i];
        cJSON *item=cJSON_CreateObject();
        ok=item!=NULL && cJSON_AddNumberToObject(item,"i",-(double)scene->id)!=NULL &&
           cJSON_AddStringToObject(item,"n",scene->name)!=NULL && cJSON_AddItemToArray(root,item);
        if(!ok) cJSON_Delete(item);
    }
    for (size_t i = 0; ok && i < passport_gallery_count; i++) {
        cJSON *item = cJSON_CreateObject();
        ok = item != NULL && cJSON_AddNumberToObject(item, "i", (double)i) != NULL &&
             cJSON_AddStringToObject(item, "n", passport_gallery_names[i]) != NULL &&
             cJSON_AddItemToArray(root, item);
        if (!ok) cJSON_Delete(item);
    }
    char *text = ok ? cJSON_PrintUnformatted(root) : NULL;
    esp_err_t result = text ? send_json(req, text) : send_json(req, "{\"ok\":false,\"error\":\"内存不足\"}");
    cJSON_free(text);
    cJSON_Delete(root);
    return result;
}

static esp_err_t select_motion(httpd_req_t *req,unsigned id) {
    if(!passport_scene_by_id(id)) return send_json(req,"{\"ok\":false,\"error\":\"此固件未包含动态场景\"}");
    if(!passport_avatar_flash_select_scene(id)) return send_json(req,"{\"ok\":false,\"error\":\"写入失败\"}");
    if(api->on_event) api->on_event(PASSPORT_CONFIG_MEDIA_CHANGED);
    return send_json(req,"{\"ok\":true}");
}
static esp_err_t post_avatar_default(httpd_req_t *req) {return select_motion(req,1);}
static esp_err_t post_avatar_animated(httpd_req_t *req) {
    char query[32],value[8],*end=NULL;
    if(httpd_req_get_url_query_str(req,query,sizeof query)!=ESP_OK ||
       httpd_query_key_value(query,"i",value,sizeof value)!=ESP_OK || !*value)
        return send_json(req,"{\"ok\":false,\"error\":\"场景无效\"}");
    unsigned long id=strtoul(value,&end,10);
    if(*end || id<1 || id>255) return send_json(req,"{\"ok\":false,\"error\":\"场景无效\"}");
    return select_motion(req,(unsigned)id);
}
static esp_err_t post_avatar_scene(httpd_req_t *req) {
    char query[32], value[8];
    int index = -1;
    if (httpd_req_get_url_query_str(req, query, sizeof query) == ESP_OK &&
        httpd_query_key_value(query, "i", value, sizeof value) == ESP_OK)
        index = atoi(value);
    if (index < 0 || (size_t)index >= passport_gallery_count)
        return send_json(req, "{\"ok\":false,\"error\":\"场景无效\"}");
    const lv_image_dsc_t *scene = &passport_gallery_frames[index];
    if (!passport_avatar_flash_begin(scene->header.w, scene->header.h) ||
        !passport_avatar_flash_chunk(scene->data, scene->data_size) ||
        !passport_avatar_flash_end())
        return send_json(req, "{\"ok\":false,\"error\":\"写入失败\"}");
    if (api->on_event) api->on_event(PASSPORT_CONFIG_MEDIA_CHANGED);
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t post_image(httpd_req_t *req) {
    char query[32], value[8];
    int width = 0, height = 0;
    if (httpd_req_get_url_query_str(req, query, sizeof query) == ESP_OK &&
        httpd_query_key_value(query, "w", value, sizeof value) == ESP_OK)
        width = atoi(value);
    if (httpd_req_get_url_query_str(req, query, sizeof query) == ESP_OK &&
        httpd_query_key_value(query, "h", value, sizeof value) == ESP_OK)
        height = atoi(value);
    size_t expected = (size_t)width * height * 2;
    if (width != PASSPORT_PICTURE_WIDTH || height != PASSPORT_PICTURE_HEIGHT ||
        expected != (size_t)req->content_len)
        return send_json(req, "{\"ok\":false,\"error\":\"尺寸无效\"}");
    if (!passport_image_flash_begin(width, height))
        return send_json(req, "{\"ok\":false,\"error\":\"存储忙\"}");
    uint8_t chunk[CHUNK_MAX];
    size_t total = 0;
    while (total < expected) {
        int got = httpd_req_recv(req, (char *)chunk, CHUNK_MAX < (int)(expected - total) ? CHUNK_MAX : (int)(expected - total));
        if (got == HTTPD_SOCK_ERR_TIMEOUT) continue;
        if (got <= 0 || !passport_image_flash_chunk(chunk, (size_t)got)) {
            passport_image_flash_abort();
            return send_json(req, "{\"ok\":false,\"error\":\"传输中断\"}");
        }
        total += (size_t)got;
    }
    if (!passport_image_flash_end())
        return send_json(req, "{\"ok\":false,\"error\":\"写入失败\"}");
    if (api->on_event) api->on_event(PASSPORT_CONFIG_MEDIA_CHANGED);
    return send_json(req, "{\"ok\":true}");
}

static esp_err_t post_image_reset(httpd_req_t *req) {
    if (!passport_image_flash_reset()) return send_json(req, "{\"ok\":false,\"error\":\"写入失败\"}");
    if (api->on_event) api->on_event(PASSPORT_CONFIG_MEDIA_CHANGED);
    return send_json(req, "{\"ok\":true}");
}

static void register_handlers(void) {
    static const httpd_uri_t routes[] = {
        {.uri = "/", .method = HTTP_GET, .handler = get_index},
        {.uri = "/api/profile", .method = HTTP_GET, .handler = get_profile},
        {.uri = "/api/profile", .method = HTTP_POST, .handler = post_profile},
        {.uri = "/api/scenes", .method = HTTP_GET, .handler = get_scenes},
        {.uri = "/api/avatar/default", .method = HTTP_POST, .handler = post_avatar_default},
        {.uri = "/api/avatar/animated", .method = HTTP_POST, .handler = post_avatar_animated},
        {.uri = "/api/avatar/scene", .method = HTTP_POST, .handler = post_avatar_scene},
        {.uri = "/api/image", .method = HTTP_POST, .handler = post_image},
        {.uri = "/api/image/reset", .method = HTTP_POST, .handler = post_image_reset},
        /* Connectivity probes that trigger the captive-portal popup. */
        {.uri = "/generate_204", .method = HTTP_GET, .handler = portal},
        {.uri = "/gen_204", .method = HTTP_GET, .handler = portal},
        {.uri = "/hotspot-detect.html", .method = HTTP_GET, .handler = portal},
        {.uri = "/library/test/success.html", .method = HTTP_GET, .handler = portal},
        {.uri = "/ncsi.txt", .method = HTTP_GET, .handler = portal},
        {.uri = "/connecttest.txt", .method = HTTP_GET, .handler = portal},
        {.uri = "/canonical.html", .method = HTTP_GET, .handler = portal},
        {.uri = "/success.txt", .method = HTTP_GET, .handler = portal},
        {.uri = "/redirect", .method = HTTP_GET, .handler = portal},
        {.uri = "/favicon.ico", .method = HTTP_GET, .handler = portal},
    };
    for (unsigned i = 0; i < sizeof routes / sizeof routes[0]; ++i) {
        httpd_uri_t wrapped=routes[i];wrapped.handler=authorized_dispatch;wrapped.user_ctx=(void *)&routes[i];
        if (httpd_register_uri_handler(server, &wrapped) != ESP_OK)
            ESP_LOGW("passport", "register uri %s failed", routes[i].uri);
    }
}

static void config_task(void *user) {
    (void)user;
    char ssid[16];
    passport_config_ssid(ssid, sizeof ssid);
    wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi;
    memset(&wifi, 0, sizeof wifi);
    strcpy((char *)wifi.ap.ssid, ssid);
    wifi.ap.ssid_len = (uint8_t)strlen(ssid);
    wifi.ap.channel = 6;
    wifi.ap.authmode = WIFI_AUTH_OPEN;
    wifi.ap.max_connection = 2;
    wifi.ap.pmf_cfg.required = false;
    esp_err_t error = ESP_OK;
    if(!api->shared_station) {
    error = esp_netif_init();
    if (error == ESP_OK) error = esp_event_loop_create_default();
    if (error == ESP_OK) netif = esp_netif_create_default_wifi_ap();
    if (netif) error = ESP_OK;
    if (error == ESP_OK) error = esp_wifi_init(&init);
    if (error == ESP_OK) error = esp_wifi_set_mode(WIFI_MODE_AP);
    if (error == ESP_OK) error = esp_wifi_set_config(WIFI_IF_AP, &wifi);
    if (error == ESP_OK) error = esp_wifi_start();
    }
    if (error == ESP_OK) {
        httpd_config_t config = HTTPD_DEFAULT_CONFIG();
        config.stack_size = 8192;
        config.max_uri_handlers = 24;
        if (httpd_start(&server, &config) == ESP_OK) {
            register_handlers();
            httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, not_found);
        } else error = ESP_FAIL;
    }
    if (error == ESP_OK && !api->shared_station) {
        dns_running = true;
        if (xTaskCreate(dns_task, "passport_dns", 3072, NULL, 4, NULL) != pdPASS)
            dns_running = false;
    }
    ESP_LOGI("cfg", "config ready, error=%d dns=%d", (int)error, (int)dns_running);
    if (error == ESP_OK) {
        (void)xSemaphoreTake(stop_signal, pdMS_TO_TICKS(CONFIG_TIMEOUT_MS));
        httpd_stop(server);
    }
    dns_running = false;
    vTaskDelay(pdMS_TO_TICKS(400)); /* Let the DNS task close its socket. */
    if(!api->shared_station) {
        esp_wifi_stop();
        esp_wifi_deinit();
        if (netif) esp_netif_destroy(netif);
        esp_event_loop_delete_default();
    }
    if (api->on_event) api->on_event(error==ESP_OK?PASSPORT_CONFIG_STOPPED:PASSPORT_CONFIG_FAILED);
    active = false;
    (void)xSemaphoreGive(exit_signal);
    vTaskDelete(NULL);
}

esp_err_t passport_config_start(const passport_config_api_t *callbacks) {
    if (!callbacks || !callbacks->submit_profile || !callbacks->submit_time)
        return ESP_ERR_INVALID_ARG;
    if (active) return ESP_ERR_INVALID_STATE;
    session_password[0]=0;
    if(callbacks->shared_station)
        snprintf(session_password,sizeof session_password,"%06x%06x",(unsigned)(esp_random()&0xffffff),(unsigned)(esp_random()&0xffffff));
    if(!passport_config_auth_init(&session_auth,callbacks->shared_station,callbacks->station_address,session_password))
        return ESP_ERR_INVALID_ARG;
    if (!stop_signal) stop_signal = xSemaphoreCreateBinary();
    if (!exit_signal) exit_signal = xSemaphoreCreateBinary();
    if (!stop_signal || !exit_signal) return ESP_ERR_NO_MEM;
    owned_api = *callbacks; /* The caller's callback struct can be on its stack. */
    while(xSemaphoreTake(stop_signal,0)==pdTRUE) {}
    while(xSemaphoreTake(exit_signal,0)==pdTRUE) {}
    shown = callbacks->snapshot;
    server = NULL;
    netif = NULL;
    active = true;
    if (xTaskCreate(config_task, "passport_cfg", 6144, NULL, 4, &task_handle) != pdPASS) {
        active = false;
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

esp_err_t passport_config_stop(void) {
    if (!active) return ESP_OK;
    (void)xSemaphoreGive(stop_signal);
    if (xSemaphoreTake(exit_signal, pdMS_TO_TICKS(8000)) != pdTRUE) return ESP_ERR_TIMEOUT;
    return ESP_OK;
}

bool passport_config_running(void) { return active; }
bool passport_config_password(char *out,size_t size) {
    if(!active || !session_auth.required || !out || size<sizeof session_password) return false;
    memcpy(out,session_password,sizeof session_password);return true;
}
