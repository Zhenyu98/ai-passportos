#include "passport_ui.h"
#include "passport_animation.h"
#include "passport_assets.h"
#include "passport_game.h"
#include "passport_strings.h"
#include "passport_version.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Pixel-arcade palette borrowed from the official FoloToy demo shell. */
#define INK 0xf4faffu
#define MUTED 0x8fa3c0u
#define NEON 0x3dff9eu
#define NEON_DIM 0x1f5e49u
#define BG_TOP 0x0e2a4au
#define BG_BOTTOM 0x0a1c33u
#define PANEL 0x0c1d3au
#define TRACK 0x1a4a6eu
#define SPARK 0x17e6c3u
#define DARK_INK 0x0a1c33u
#define DARK_MUTED 0x4a5a76u
#define BAT_YELLOW 0xffcf3fu
#define BAT_RED 0xff5a5au

/* ---- Visual tokens: the single source of truth for layout geometry. ---- */
/* Screen and body regions. */
#define SCREEN_W 240
#define SCREEN_H 320
#define BODY_Y 30
#define BODY_W 240
#define BODY_H (SCREEN_H - BODY_Y) /* 290 */
/* Shared content margin/width for detail-page text. */
#define CONTENT_X 22
#define CONTENT_W 196

/* Status bar. */
#define CLOCK_X 6
#define CLOCK_Y 1
#define CLOCK_W 90
#define CLOCK_H 30
#define WEEKDAY_X 100
#define WEEKDAY_Y 10
#define WEEKDAY_W 34
#define WEEKDAY_H 16
#define BAT_TEXT_X 152
#define BAT_TEXT_Y 9
#define BAT_TEXT_W 48
#define BAT_TEXT_H 18
#define BAT_BODY_X 206
#define BAT_BODY_Y 11
#define BAT_BODY_W 20
#define BAT_BODY_H 12
#define BAT_NUB_X 226
#define BAT_NUB_Y 15
#define BAT_NUB_W 3
#define BAT_NUB_H 5
#define BAT_FILL_X 207
#define BAT_FILL_Y 12
#define BAT_FILL_W 18
#define BAT_FILL_H 10

/* Heading strip on detail pages (body-local). */
#define HEADING_X CONTENT_X
#define HEADING_Y 0
#define HEADING_W CONTENT_W
#define HEADING_H 24

/* Badge page (body-local). */
#define AVATAR_COL_X 6
#define AVATAR_COL_Y 22
#define AVATAR_STACK_X 12
#define AVATAR_STACK_Y 8
#define TEMP_X 6
#define TEMP_Y 176
#define TEMP_W 76
#define INFO_COL_X 88
#define INFO_COL_W 144
#define INFO_STACK_X 12
#define INFO_STACK_W 216
#define INFO_NAME_Y_COL 30
#define INFO_TITLE_Y_COL 90
#define INFO_RESEARCH_Y_COL 154
#define INFO_NAME_Y_STACK 124
#define INFO_TITLE_Y_STACK 164
#define INFO_RESEARCH_Y_STACK 202

/* Badge bottom buttons (body-local). */
#define BTN_Y 236
#define BTN_W 108
#define BTN_H 44
#define BTN_AGENT_X 8
#define BTN_QR_X 124

/* Menu rows (body-local). */
#define MENU_X 12
#define MENU_Y0 32
#define MENU_W 216
#define MENU_ROW_H 44
#define MENU_GAP 50

/* Picture page hint (on the white full-bleed canvas). */
#define HINT_TITLE_Y 140
#define HINT_SUB_Y 176

/* Brightness page (body-local). */
#define BRIGHT_VALUE_Y 60
#define BRIGHT_VALUE_H 30
#define BRIGHT_BAR_X0 28
#define BRIGHT_BAR_GAP 63
#define BRIGHT_BAR_Y 116
#define BRIGHT_BAR_W 58
#define BRIGHT_BAR_H 8
#define BRIGHT_HINT_Y 148

/* About page (body-local). */
#define ABOUT_TITLE_Y 60
#define ABOUT_VER_Y 100
#define ABOUT_BOARD_Y 136

static lv_obj_t *screen,*body,*clock_label,*battery_label,*weekday_label,*avatar;
static lv_obj_t *battery_body,*battery_fill,*battery_nub;
static lv_obj_t *previous_screen;
static lv_timer_t *avatar_timer;
static passport_animation_t animation;
static const passport_motion_scene_t *avatar_scene;
static uint32_t scene_began;
static bool avatar_motion,avatar_temporary,pending_ack,have_model;
static lv_obj_t *badge_buttons[2];
static passport_model_t last_model;
static passport_profile_t last_profile;
static bool rendered;
static passport_agent_model_t agent_view;
static lv_obj_t *agent_content;
static lv_obj_t *agent_stage,*agent_reply_label,*agent_hint,*agent_note;
static uint32_t agent_visual_since;
static bool agent_dirty;
static unsigned note_index;
static char setup_web[96];
static char status_text[64];
static uint32_t status_tick;
static unsigned blink_tick;
static bool page_failed;
/* Status-bar redraw caches: only push text/icon updates when the value changed. */
static char last_clock[32]="";
static char last_level[12]="";
static int last_weekday=-2;
static int last_battery=-2;
/* Game page state: declared here so clear_page can reset it. The whole
 * playfield is painted by one object's draw event, keeping the game page
 * inside the 32 KB LVGL pool shared with every other page. */
static passport_game_t game;
static bool game_page;
static uint32_t game_tick;
static int game_overlay_state;
static uint32_t game_shown_score,game_shown_coins,game_shown_time;
static uint8_t game_shown_world;
static lv_obj_t *game_field;
static lv_obj_t *game_hud_score,*game_hud_coins,*game_hud_world,*game_hud_time;
static lv_obj_t *game_overlay,*game_overlay_title,*game_overlay_line,
                *game_overlay_hint,*game_overlay_hint2;


static lv_obj_t *box(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color) {
    lv_obj_t *obj=lv_obj_create(parent);
    if (!obj) {page_failed=true;return NULL;}
    lv_obj_remove_style_all(obj);
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_bg_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_bg_opa(obj,LV_OPA_COVER,0);
    lv_obj_remove_flag(obj,LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}
static lv_obj_t *panel(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *obj=box(parent,x,y,w,h,color);
    if (obj) lv_obj_set_style_radius(obj,radius<0 ? LV_RADIUS_CIRCLE : radius,0);
    return obj;
}
static lv_obj_t *outline(lv_obj_t *parent,int x,int y,int w,int h,uint32_t color,int radius) {
    lv_obj_t *obj=panel(parent,x,y,w,h,color,radius);
    if (obj) {
        lv_obj_set_style_bg_opa(obj,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_color(obj,lv_color_hex(NEON),0);
        lv_obj_set_style_border_width(obj,2,0);
    }
    return obj;
}
static lv_obj_t *text(lv_obj_t *parent,int x,int y,int w,int h,const char *value,
                      const lv_font_t *font,uint32_t color,bool centered) {
    if(!parent) {page_failed=true;return NULL;}
    lv_obj_t *obj=lv_label_create(parent);
    if (!obj) {page_failed=true;return NULL;}
    lv_obj_set_pos(obj,x,y); lv_obj_set_size(obj,w,h);
    lv_obj_set_style_text_font(obj,font,0);
    lv_obj_set_style_text_color(obj,lv_color_hex(color),0);
    lv_obj_set_style_text_align(obj,centered ? LV_TEXT_ALIGN_CENTER:LV_TEXT_ALIGN_LEFT,0);
    lv_label_set_long_mode(obj,LV_LABEL_LONG_MODE_DOTS);
    lv_label_set_text(obj,value);
    return obj;
}
/* Keep one full font line visible. Only horizontal overflow needs motion;
 * a short field must not trigger LVGL's vertical overflow animation. */
static void info_text(int x,int y,int w,int h,const char *value,const lv_font_t *font,uint32_t color) {
    char single_line[PASSPORT_RESEARCH_MAX+1];
    snprintf(single_line,sizeof single_line,"%s",value);
    for(char *p=single_line;*p;p++) if(*p=='\n' || *p=='\r') *p=' ';
    int line_height=lv_font_get_line_height(font);
    lv_obj_t *obj=text(body,x,y,w,h<line_height?line_height:h,single_line,font,color,true);
    if(obj) {
        lv_point_t size;
        lv_text_get_size(&size,single_line,font,0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
        lv_label_set_long_mode(obj,size.x>w?LV_LABEL_LONG_MODE_SCROLL:LV_LABEL_LONG_MODE_CLIP);
        lv_obj_set_style_anim_duration(obj,lv_anim_speed(28),0);
    }
}
static bool covered(const char *value) {
    const unsigned char *p=(const unsigned char *)value;
    while (*p) {
        uint32_t cp=*p++; unsigned extra=0;
        if (cp>=0xf0) {cp&=7; extra=3;}
        else if(cp>=0xe0) {cp&=15;extra=2;}
        else if(cp>=0xc0) {cp&=31;extra=1;}
        while(extra--) {if(!*p) return false;cp=(cp<<6)|(*p++&63);}
        if(cp!=10 && !passport_assets_has_glyph(cp,NULL)) return false;
    }
    return true;
}
static const char *field(const char *value,const char *unset) {
    return !*value ? unset:covered(value) ? value:PASSPORT_TEXT_GLYPHS_MISSING;
}
/* Pre-rendered Flash frames keep a stable position and opacity. The neutral
 * public placeholder can still blink; uploaded static scenes stay static. */
static void animate(lv_timer_t *timer) {
    (void)timer;
    if(!avatar) return;
    const lv_image_dsc_t *next=NULL;
    if(agent_stage) {
        next=passport_agent_avatar_frame(passport_animation_nora_frame(agent_view.state,agent_view.voice,
            lv_tick_get()-agent_visual_since,agent_view.level));
    } else if(avatar_motion) {
        unsigned frame=avatar_scene->interactive?passport_animation_frame(&animation,lv_tick_get()):
            passport_animation_loop_frame(avatar_scene->durations,(unsigned)avatar_scene->frame_count,scene_began,lv_tick_get());
        if(frame<avatar_scene->frame_count) next=&avatar_scene->frames[frame];
    } else if(avatar_temporary) {
        unsigned phase=blink_tick++%42;
        unsigned frame=phase==37 || phase==39 ? 1:phase==38 ? 2:0;
        next=&passport_avatar_frames[frame];
    }
    if(next && lv_image_get_src(avatar)!=next) lv_image_set_src(avatar,next);
}
static void clear_page(void) {
    agent_content=NULL;
    agent_stage=agent_reply_label=agent_hint=agent_note=NULL;
    game_page=false;
    if(avatar_timer) {lv_timer_delete(avatar_timer);avatar_timer=NULL;}
    passport_animation_hide(&animation);
    avatar_motion=avatar_temporary=false;
    avatar_scene=NULL;
    badge_buttons[0]=badge_buttons[1]=NULL;
    avatar=NULL;
    if(body) lv_obj_clean(body);
}
static void label_changed(lv_obj_t *obj,const char *value) {
    if(obj && strcmp(lv_label_get_text(obj),value)) lv_label_set_text(obj,value);
}
static void agent_update(void) {
    if(!agent_stage || !agent_dirty) return;
    agent_dirty=false;
    bool speaking=agent_view.voice==PASSPORT_VOICE_PLAYING || agent_view.voice==PASSPORT_VOICE_PREPARING;
    const char *content=agent_view.reply[0]?agent_view.reply:agent_view.heard;
    if(agent_view.state==PASSPORT_AGENT_CONFIRM)
        content="确认是你发起的配对，\n再按 OK 允许连接。";
    else if(!*content) {
        switch(agent_view.state) {
            case PASSPORT_AGENT_UNAVAILABLE:content="请安装 Muse 接入固件";break;
            case PASSPORT_AGENT_UNPAIRED:content="请在 Muse 手机应用中添加设备";break;
            case PASSPORT_AGENT_CONNECTING:content="正在连接…";break;
            case PASSPORT_AGENT_OFFLINE:content="网络已断开，请检查 Wi-Fi";break;
            case PASSPORT_AGENT_RECORDING:content="我在听…";break;
            case PASSPORT_AGENT_WAITING:content="正在思考…";break;
            case PASSPORT_AGENT_ERROR:content=agent_view.detail[0]?agent_view.detail:"操作失败，请重试";break;
            default:break;
        }
    }
    if(strcmp(lv_label_get_text(agent_reply_label),content)) {
        lv_label_set_text(agent_reply_label,content);
        lv_obj_update_layout(agent_content);
        int32_t max_scroll=lv_obj_get_height(agent_reply_label)-lv_obj_get_content_height(agent_content);
        if(max_scroll<0) max_scroll=0;
        if(lv_obj_get_scroll_y(agent_content)>max_scroll) lv_obj_scroll_to_y(agent_content,max_scroll,LV_ANIM_OFF);
    }
    const char *note="";
    if(*status_text) note=status_text;
    else if(agent_view.state==PASSPORT_AGENT_CONFIRM) { /* Consent stays focused on the phone request. */ }
    else if(agent_view.state==PASSPORT_AGENT_ERROR && agent_view.detail[0]) {
        if(content!=agent_view.detail) note=agent_view.detail;
    } else if(agent_view.connection!=PASSPORT_AGENT_READY) {
        /* A retained transcript must not hide why another turn is unavailable. */
        if(agent_view.reply[0] || agent_view.heard[0] || agent_view.state!=agent_view.connection) {
            switch(agent_view.connection) {
                case PASSPORT_AGENT_UNAVAILABLE:note="需要 Muse 接入固件";break;
                case PASSPORT_AGENT_UNPAIRED:note="请先在手机配对";break;
                case PASSPORT_AGENT_OFFLINE:note="网络已断开";break;
                default:note="正在连接…";break;
            }
        }
    } else if(agent_view.detail[0] && content!=agent_view.detail) note=agent_view.detail;
    else if(agent_view.voice_detail[0]) note=agent_view.voice_detail;
    else if(agent_view.voice==PASSPORT_VOICE_UNCONFIGURED)
        note=agent_view.reply[0]?"仅文字回复 · 完整见手机":"仅文字回复";
    else if(agent_view.reply[0]) note="回复预览 · 完整见手机";
    label_changed(agent_note,note);
    const char *hint=agent_view.state==PASSPORT_AGENT_CONFIRM?"按 OK 确认配对":
        agent_view.state==PASSPORT_AGENT_RECORDING?"按 OK 发送":
        agent_view.state==PASSPORT_AGENT_WAITING?"":speaking?"按 OK 停止朗读":
        agent_view.connection==PASSPORT_AGENT_READY?"按 OK 说话":"";
    label_changed(agent_hint,hint);animate(NULL);
}
static bool agent_build(void) {
    text(body,12,16,216,30,passport_agent_name,&passport_font_20,DARK_INK,true);
    agent_stage=box(body,0,46,240,150,0xffffff);
    if(!agent_stage) return false;
    lv_obj_set_style_bg_opa(agent_stage,LV_OPA_TRANSP,0);
    const lv_image_dsc_t *portrait=passport_agent_avatar_frame(0);
    avatar=lv_image_create(agent_stage);
    if(!avatar) {page_failed=true;return false;}
    lv_image_set_src(avatar,portrait);
    /* Native Flash frames avoid a scaled image layer on the no-PSRAM C3. */
    lv_obj_set_pos(avatar,(240-(int)portrait->header.w)/2,(150-(int)portrait->header.h)/2);
    avatar_scene=NULL;avatar_motion=avatar_temporary=false;
    scene_began=agent_visual_since=lv_tick_get();blink_tick=0;
    agent_content=lv_obj_create(body);
    if(!agent_content) {page_failed=true;return false;}
    lv_obj_set_pos(agent_content,16,202);lv_obj_set_size(agent_content,208,66);
    lv_obj_set_style_bg_opa(agent_content,LV_OPA_TRANSP,0);lv_obj_set_style_border_width(agent_content,0,0);
    lv_obj_set_style_pad_all(agent_content,2,0);
    agent_reply_label=lv_label_create(agent_content);
    if(!agent_reply_label) {page_failed=true;return false;}
    lv_obj_set_width(agent_reply_label,204);lv_label_set_long_mode(agent_reply_label,LV_LABEL_LONG_MODE_WRAP);
    lv_obj_set_style_text_font(agent_reply_label,&passport_font_20,0);
    lv_obj_set_style_text_align(agent_reply_label,LV_TEXT_ALIGN_CENTER,0);
    lv_obj_set_style_text_color(agent_reply_label,lv_color_hex(0x222222),0);
    agent_note=text(body,12,272,216,20,"",&passport_font_16,0x777777,true);
    agent_hint=text(body,12,294,216,22,"",&passport_font_16,0x555555,true);
    avatar_timer=lv_timer_create(animate,100,NULL);if(!avatar_timer) page_failed=true;
    agent_dirty=true;agent_update();return !page_failed;
}
bool passport_ui_create(void) {
    if(screen) return true;
    previous_screen=lv_screen_active();
    screen=box(NULL,0,0,SCREEN_W,SCREEN_H,BG_TOP);
    if(!screen) return false;
    lv_obj_set_style_bg_grad_color(screen,lv_color_hex(BG_BOTTOM),0);
    lv_obj_set_style_bg_grad_dir(screen,LV_GRAD_DIR_VER,0);
    clock_label=text(screen,CLOCK_X,CLOCK_Y,CLOCK_W,CLOCK_H,"--:--",&lv_font_montserrat_28,INK,false);
    battery_label=text(screen,BAT_TEXT_X,BAT_TEXT_Y,BAT_TEXT_W,BAT_TEXT_H,"--%",&lv_font_montserrat_14,INK,false);
    lv_obj_set_style_text_align(battery_label,LV_TEXT_ALIGN_RIGHT,0);
    body=box(screen,0,BODY_Y,BODY_W,BODY_H,BG_TOP);
    if(!clock_label || !battery_label || !body) {passport_ui_destroy();return false;}
    lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
    weekday_label=text(screen,WEEKDAY_X,WEEKDAY_Y,WEEKDAY_W,WEEKDAY_H,"---",&lv_font_montserrat_14,MUTED,false);
    battery_body=box(screen,BAT_BODY_X,BAT_BODY_Y,BAT_BODY_W,BAT_BODY_H,PANEL);
    battery_nub=box(screen,BAT_NUB_X,BAT_NUB_Y,BAT_NUB_W,BAT_NUB_H,INK);
    battery_fill=box(screen,BAT_FILL_X,BAT_FILL_Y,0,BAT_FILL_H,NEON);
    if(!weekday_label || !battery_body || !battery_nub || !battery_fill) {passport_ui_destroy();return false;}
    lv_obj_set_style_bg_opa(battery_body,LV_OPA_TRANSP,0);
    lv_obj_set_style_border_color(battery_body,lv_color_hex(INK),0);
    lv_obj_set_style_border_width(battery_body,1,0);
    lv_obj_set_style_radius(battery_body,2,0);
    lv_obj_set_style_radius(battery_fill,1,0);
    rendered=false; have_model=false; pending_ack=false; status_text[0]=0;
    last_clock[0]=0; last_level[0]=0; last_weekday=-2; last_battery=-2;
    lv_screen_load(screen);
    return true;
}
static void agent_icon(lv_obj_t *parent,int x,int y,uint32_t color) {
    box(parent,x,y,4,22,color);
    box(parent,x+4,y+5,4,12,color);
    box(parent,x+8,y+9,4,5,color);
}
static void qr_icon(lv_obj_t *parent,int x,int y,uint32_t color) {
    box(parent,x,y,7,7,color);
    box(parent,x+11,y,7,7,color);
    box(parent,x,y+11,7,7,color);
    box(parent,x+11,y+11,5,5,color);
}
static void button_focus(unsigned index,bool selected) {
    lv_obj_t *obj=badge_buttons[index];
    if(!obj) return;
    uint32_t color=selected?NEON:MUTED;
    lv_obj_set_style_outline_opa(obj,selected?LV_OPA_90:LV_OPA_30,0);
    for(unsigned i=0;i<lv_obj_get_child_count(obj);i++) {
        lv_obj_t *child=lv_obj_get_child(obj,i);
        if(lv_obj_check_type(child,&lv_label_class)) lv_obj_set_style_text_color(child,lv_color_hex(color),0);
        else lv_obj_set_style_bg_color(child,lv_color_hex(color),0);
    }
}
static bool button(int x,int y,int w,int h,const char *label,int icon,bool selected) {
    lv_obj_t *obj=outline(body,x,y,w,h,PANEL,14);
    if(!obj) return false;
    uint32_t color=selected?NEON:MUTED;
    lv_obj_set_style_outline_width(obj,3,0);
    lv_obj_set_style_outline_color(obj,lv_color_hex(NEON),0);
    lv_obj_set_style_outline_opa(obj,selected?LV_OPA_90:LV_OPA_30,0);
    text(obj,34,(h-20)/2,w-42,20,label,&lv_font_montserrat_14,color,true);
    if(icon) qr_icon(obj,12,(h-18)/2,color); else agent_icon(obj,12,(h-22)/2,color);
    badge_buttons[icon?1:0]=obj;
    return true;
}
/* Bottom chrome: the badge shows two selectable action buttons; every other
 * page is a detail view with no bottom buttons. */
static bool chrome(const passport_model_t *m) {
    if(m->page==PASSPORT_PAGE_PICTURE || m->page==PASSPORT_PAGE_GAME) return true;
    if(m->page==PASSPORT_PAGE_BADGE) {
        if(!button(BTN_AGENT_X,BTN_Y,BTN_W,BTN_H,"AGENT",0,m->badge_button==0)) return false;
        if(!button(BTN_QR_X,BTN_Y,BTN_W,BTN_H,"QR",1,m->badge_button==1)) return false;
    }
    return true;
}
/* ---- SMB-style game page: landscape rendering of the pure passport_game
 * logic. The game world is a 320x240 landscape viewport; the portrait panel
 * shows it rotated a quarter turn counterclockwise so a sideways grip maps
 * UP to left, DOWN to right and OK to jump. A world-space rect (gx,gy,gw,gh)
 * lands at screen (239-gy-gh, gx) with swapped extents. The whole playfield
 * is painted by ONE object's draw event issuing lv_draw_rect calls: the
 * 32 KB LVGL pool is shared with every other page and only has room for a
 * handful of labels on top of the painter. ---- */
#define G_SKY 0x5c94fcu
#define G_GROUND 0xc86820u
#define G_GROUND_EDGE 0x5a2c10u
#define G_BRICK 0xa84818u
#define G_BRICK_EDGE 0x402008u
#define G_QBLOCK 0xe8a018u
#define G_QBLOCK_EDGE 0x7a4a00u
#define G_USED 0x8a5a30u
#define G_STAIR 0xc0501au
#define G_STAIR_EDGE 0x5a2c10u
#define G_WALL 0x808080u
#define G_WALL_EDGE 0x404040u
#define G_PIPE 0x00a800u
#define G_PIPE_EDGE 0x005800u
#define G_GOOMBA 0xa04010u
#define G_GOOMBA_DARK 0x502800u
#define G_MUSH_CAP 0xd82800u
#define G_MUSH_SPOT 0xffffffu
#define G_MUSH_STEM 0xfce0a8u
#define G_COIN 0xffd23fu
#define G_COIN_EDGE 0xc58f00u
#define G_RED 0xd82800u
#define G_BLUE 0x0058f8u
#define G_SKIN 0xffd7a8u
#define G_SHOE 0x6a3410u
#define G_EYE 0x302010u
#define G_POLE 0x00a800u
#define G_FLAG 0x00d800u
#define G_CASTLE 0xb45f20u
#define G_DOOR 0x000000u
#define G_CLOUD 0xffffffu
#define G_HILL 0x00a800u
#define G_TEXT 0xffffffu

static void game_hide(lv_obj_t *obj) {
    if(obj) lv_obj_add_flag(obj,LV_OBJ_FLAG_HIDDEN);
}
/* A landscape label lays its text along the long axis first, then spins the
 * whole label a quarter turn clockwise about its centre, so the text reads
 * left-to-right for a holder gripping the portrait panel sideways. The
 * unrotated label's centre must coincide with the rotated rect's centre. */
static lv_obj_t *glabel(int gx,int gy,int gw,int gh,const lv_font_t *font,
                        uint32_t color) {
    lv_obj_t *obj=text(body,239-gy-(gh+gw)/2,gx+(gw-gh)/2,gw,gh,"",font,color,true);
    if(obj) {
        lv_obj_set_style_transform_rotation(obj,900,0);
        lv_obj_set_style_transform_pivot_x(obj,gw/2,0);
        lv_obj_set_style_transform_pivot_y(obj,gh/2,0);
    }
    return obj;
}
/* Fill one world-space rect through LVGL's draw pipeline. */
static void grect(lv_layer_t *layer,int gx,int gy,int gw,int gh,uint32_t color) {
    lv_draw_rect_dsc_t dsc;
    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_color=lv_color_hex(color);
    dsc.bg_opa=LV_OPA_COVER;
    lv_area_t a;
    a.x1=239-gy-gh; a.y1=gx; a.x2=239-gy-1; a.y2=gx+gw-1;
    lv_draw_rect(layer,&dsc,&a);
}
/* Paint the whole world into the layer. Reads the game view only. */
static void game_paint(lv_layer_t *layer) {
    int cam=game.cam_q8>>8;
    uint32_t now=lv_tick_get();
    /* Sky furniture: repeating clouds, hills and bushes at world offsets. */
    unsigned di=0;
    for(unsigned n=0;n<20 && di<3;n++) {
        int vx=40+(int)n*176-cam;
        if(vx>-40 && vx<320) {
            int y=20+(int)(n%3)*24;
            grect(layer,vx,y,34,12,G_CLOUD);
            grect(layer,vx+4,y-4,12,4,G_CLOUD);
            grect(layer,vx+18,y-4,12,4,G_CLOUD);
            di++;
        }
    }
    for(unsigned n=0;n<17 && di<5;n++) {
        int vx=16+(int)n*208-cam;
        if(vx>-48 && vx<320) {
            int h=(n&1)?18:28;
            grect(layer,vx,208-h,44,h,G_HILL);
            grect(layer,vx+10,208-h-4,10,4,G_HILL);
            di++;
        }
    }
    for(unsigned n=0;n<18 && di<7;n++) {
        int vx=88+(int)n*184-cam;
        if(vx>-34 && vx<320) {
            grect(layer,vx,198,30,10,G_HILL);
            grect(layer,vx+4,194,8,4,G_HILL);
            grect(layer,vx+18,194,8,4,G_HILL);
            di++;
        }
    }
    /* Solid tiles, merged into horizontal runs per visible row. Pipe tiles
     * are skipped here; pipes render from game.pipes below. */
    int c0=cam/16,c1=(cam+319)/16;
    if(c0<0) c0=0;
    if(c1>=PASSPORT_GAME_COLS) c1=PASSPORT_GAME_COLS-1;
    for(int row=0;row<PASSPORT_GAME_ROWS;row++) {
        int run=-1;
        uint8_t type=0;
        for(int c=c0;c<=c1+1;c++) {
            uint8_t t=c<=c1?game.tile[row][c]:PASSPORT_TILE_EMPTY;
            if(t>=PASSPORT_TILE_PIPE_TL) t=PASSPORT_TILE_EMPTY;
            if(c<=c1 && t!=PASSPORT_TILE_EMPTY && run>=0 && t==type) continue;
            if(run>=0) {
                int x=run*16-cam,w=(c-run)*16;
                switch(type) {
                    case PASSPORT_TILE_BRICK:
                        grect(layer,x,row*16,w,16,G_BRICK);
                        grect(layer,x,row*16+15,w,1,G_BRICK_EDGE);
                        break;
                    case PASSPORT_TILE_QCOIN:
                    case PASSPORT_TILE_QMUSH:
                        grect(layer,x,row*16,w,16,G_QBLOCK);
                        grect(layer,x,row*16+15,w,1,G_QBLOCK_EDGE);
                        break;
                    case PASSPORT_TILE_USED:
                        grect(layer,x,row*16,w,16,G_USED);
                        break;
                    case PASSPORT_TILE_STAIR:
                        grect(layer,x,row*16,w,16,G_STAIR);
                        break;
                    case PASSPORT_TILE_SOLID:
                        grect(layer,x,row*16,w,16,G_WALL);
                        break;
                    default:
                        grect(layer,x,row*16,w,16,G_GROUND);
                        if(row==13) grect(layer,x,row*16,w,2,G_GROUND_EDGE);
                        break;
                }
            }
            run=c<=c1 && t!=PASSPORT_TILE_EMPTY?c:-1;
            type=t;
        }
    }
    /* Pipes: a wider rim on top of a straight body, from the pipe table. */
    for(unsigned p=0;p<game.pipe_count;p++) {
        int x=game.pipes[p].col*16-cam;
        if(x<-40||x>=320) continue;
        int top=(13-game.pipes[p].height)*16;
        grect(layer,x-2,top,36,16,G_PIPE);
        grect(layer,x,top,32,2,G_PIPE_EDGE);
        grect(layer,x,top+16,28,208-(top+16),G_PIPE);
        grect(layer,x,top+16,2,208-(top+16),G_PIPE_EDGE);
    }
    /* Flagpole and castle. */
    int fx=game.flag_col*16+7-cam;
    if(fx>-16 && fx<320) {
        grect(layer,fx,48,2,144,G_POLE);
        grect(layer,fx-10,game.flag_y_q8>>8,10,8,G_FLAG);
    }
    int cx=game.castle_col*16-cam;
    if(cx>-56 && cx<320) {
        grect(layer,cx,180,48,28,G_CASTLE);
        grect(layer,cx+18,194,12,14,G_DOOR);
        grect(layer,cx+12,166,24,14,G_CASTLE);
        grect(layer,cx+12,160,6,6,G_CASTLE);
        grect(layer,cx+18,160,6,6,G_CASTLE);
        grect(layer,cx+24,160,6,6,G_CASTLE);
    }
    /* Coins spin by squeezing their width. */
    unsigned spin=(now/150)%2;
    for(unsigned i=0;i<PASSPORT_GAME_COIN_POOL;i++) {
        const passport_game_coin_t *c=&game.coin[i];
        if(!c->active) continue;
        int x=(c->x_q8>>8)-cam;
        if(x<-16||x>=320) continue;
        grect(layer,x+(spin?0:4),c->y,spin?12:4,12,G_COIN);
        grect(layer,x+(spin?0:4),c->y,spin?12:4,2,G_COIN_EDGE);
    }
    /* Goombas: dome plus dark feet; squashed ones flatten. */
    for(unsigned i=0;i<PASSPORT_GAME_GOOMBA_POOL;i++) {
        const passport_game_goomba_t *e=&game.goombas[i];
        if(!e->active) continue;
        int x=(e->x_q8>>8)-cam;
        if(x<-20||x>=320) continue;
        int y=e->y_q8>>8;
        if(e->squash_ms) grect(layer,x,y+9,14,5,G_GOOMBA);
        else {
            grect(layer,x,y,14,10,G_GOOMBA);
            grect(layer,x,y+10,14,4,G_GOOMBA_DARK);
        }
    }
    /* Mushroom: red cap with a white spot on a cream stem. */
    const passport_game_mushroom_t *m=&game.mushroom;
    if(m->active) {
        int x=(m->x_q8>>8)-cam,y=m->y_q8>>8;
        grect(layer,x,y,14,8,G_MUSH_CAP);
        grect(layer,x+5,y+2,4,4,G_MUSH_SPOT);
        grect(layer,x+2,y+8,10,6,G_MUSH_STEM);
    }
    for(unsigned i=0;i<PASSPORT_GAME_FRAGMENT_POOL;i++) {
        const passport_game_fragment_t *f=&game.frag[i];
        if(f->active) grect(layer,(f->x_q8>>8)-cam,f->y_q8>>8,6,6,G_BRICK);
    }
    for(unsigned i=0;i<PASSPORT_GAME_POPCOIN_POOL;i++) {
        const passport_game_popcoin_t *p=&game.popcoin[i];
        if(p->active) grect(layer,(p->x_q8>>8)-cam,p->y_q8>>8,8,12,G_COIN);
    }
    /* Player: cap-face-eye-suit-shoes, blinking while invulnerable. */
    if(!(game.invuln_ms && (now/100)%2==0)) {
        int x=(game.px_q8>>8)-cam,y=game.py_q8>>8;
        if(game.big) {
            grect(layer,x,y,12,5,G_RED);
            grect(layer,x,y+5,12,6,G_SKIN);
            grect(layer,x+(game.facing>0?8:2),y+6,2,2,G_EYE);
            grect(layer,x,y+11,12,12,G_BLUE);
            grect(layer,x,y+23,12,4,G_SHOE);
        } else {
            grect(layer,x,y,12,4,G_RED);
            grect(layer,x,y+4,12,5,G_SKIN);
            grect(layer,x+(game.facing>0?8:2),y+5,2,2,G_EYE);
            grect(layer,x,y+9,12,4,G_BLUE);
            grect(layer,x,y+13,12,2,G_SHOE);
        }
    }
    /* HUD coin icon next to the coin counter. */
    grect(layer,114,5,8,8,G_COIN);
    grect(layer,114,5,8,2,G_COIN_EDGE);
}
static void game_draw_event(lv_event_t *e) {
    if(lv_event_get_code(e)!=LV_EVENT_DRAW_MAIN || !game_page) return;
    game_paint(lv_event_get_layer(e));
}
static bool game_build(void) {
    lv_obj_set_style_bg_color(body,lv_color_hex(G_SKY),0);
    lv_obj_set_style_bg_opa(body,LV_OPA_COVER,0);
    lv_obj_set_style_bg_grad_dir(body,LV_GRAD_DIR_NONE,0);
    game_field=box(body,0,0,SCREEN_W,SCREEN_H,G_SKY);
    if(game_field) {
        lv_obj_set_style_bg_opa(game_field,LV_OPA_TRANSP,0);
        lv_obj_add_event_cb(game_field,game_draw_event,LV_EVENT_DRAW_MAIN,NULL);
        game_hide(game_field);
    }
    /* HUD labels across the top of the landscape view. */
    game_hud_score=glabel(8,2,100,16,&passport_font_16,G_TEXT);
    game_hud_coins=glabel(126,2,40,16,&passport_font_16,G_TEXT);
    game_hud_world=glabel(172,2,40,16,&passport_font_16,G_TEXT);
    game_hud_time=glabel(216,2,72,16,&passport_font_16,G_TEXT);
    game_overlay=panel(body,239-64-112,56,112,208,0x0a1c33,10);
    if(game_overlay) {
        lv_obj_set_style_border_color(game_overlay,lv_color_hex(NEON),0);
        lv_obj_set_style_border_width(game_overlay,2,0);
        game_hide(game_overlay);
    }
    game_overlay_title=glabel(64,76,192,24,&passport_font_20,NEON);
    game_overlay_line=glabel(64,106,192,18,&passport_font_16,INK);
    game_overlay_hint=glabel(64,130,192,16,&passport_font_16,MUTED);
    game_overlay_hint2=glabel(64,150,192,16,&passport_font_16,MUTED);
    if(page_failed) return false;
    passport_game_init(&game,lv_tick_get());
    game_page=true; game_tick=0;
    game_overlay_state=-1;
    game_shown_score=game_shown_coins=game_shown_time=0xffffffffu;
    game_shown_world=0xff;
    if(game_field) lv_obj_clear_flag(game_field,LV_OBJ_FLAG_HIDDEN);
    return true;
}
static void game_overlay_update(void) {
    int wanted=game.state==PASSPORT_GAME_READY?1:
               game.state==PASSPORT_GAME_OVER?2:
               (game.state==PASSPORT_GAME_CLEAR &&
                game.clear_phase==PASSPORT_CLEAR_POSE && game.clear_ms>400)?3:0;
    if(wanted==game_overlay_state && (wanted==0 || game_shown_score==game.score)) return;
    if(!game_overlay) {game_overlay_state=wanted; return;}
    if(wanted==0) {
        game_hide(game_overlay);game_hide(game_overlay_title);
        game_hide(game_overlay_line);game_hide(game_overlay_hint);
        game_hide(game_overlay_hint2);
    } else {
        char line[48];
        lv_obj_clear_flag(game_overlay,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(game_overlay_title,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(game_overlay_line,LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(game_overlay_hint,LV_OBJ_FLAG_HIDDEN);
        if(wanted==3) game_hide(game_overlay_hint2);
        else lv_obj_clear_flag(game_overlay_hint2,LV_OBJ_FLAG_HIDDEN);
        if(wanted==1) {
            lv_label_set_text(game_overlay_title,PASSPORT_TEXT_MENU_GAME);
            snprintf(line,sizeof line,"%s x%u · %s %lu",PASSPORT_TEXT_GAME_LIVES,
                     game.lives,PASSPORT_TEXT_GAME_BEST,(unsigned long)game.best);
            lv_label_set_text(game_overlay_hint,PASSPORT_TEXT_GAME_READY);
            lv_label_set_text(game_overlay_hint2,PASSPORT_TEXT_GAME_EXIT);
        } else if(wanted==2) {
            lv_label_set_text(game_overlay_title,PASSPORT_TEXT_GAME_OVER);
            snprintf(line,sizeof line,"%s %lu · %s %lu",PASSPORT_TEXT_GAME_SCORE,
                     (unsigned long)game.score,PASSPORT_TEXT_GAME_BEST,(unsigned long)game.best);
            lv_label_set_text(game_overlay_hint,PASSPORT_TEXT_GAME_RESTART);
            lv_label_set_text(game_overlay_hint2,PASSPORT_TEXT_GAME_EXIT);
        } else {
            lv_label_set_text(game_overlay_title,PASSPORT_TEXT_GAME_CLEAR);
            snprintf(line,sizeof line,"%s %lu · %s %lu",PASSPORT_TEXT_GAME_SCORE,
                     (unsigned long)game.score,PASSPORT_TEXT_GAME_BEST,(unsigned long)game.best);
            lv_label_set_text(game_overlay_hint,PASSPORT_TEXT_GAME_NEXT);
        }
        lv_label_set_text(game_overlay_line,line);
    }
    game_overlay_state=wanted;
}
static void game_frame(void) {
    uint32_t now=lv_tick_get();
    uint32_t dt=game_tick?lv_tick_elaps(game_tick):33;
    game_tick=now;
    passport_game_step(&game,dt);
    char buf[40];
    if(game_shown_score!=game.score) {
        snprintf(buf,sizeof buf,"%s %06lu",PASSPORT_TEXT_GAME_SCORE,(unsigned long)game.score);
        if(game_hud_score) lv_label_set_text(game_hud_score,buf);
        game_shown_score=game.score;
    }
    if(game_shown_coins!=game.coins) {
        snprintf(buf,sizeof buf,"x%02u",game.coins);
        if(game_hud_coins) lv_label_set_text(game_hud_coins,buf);
        game_shown_coins=game.coins;
    }
    if(game_shown_world!=game.loop) {
        snprintf(buf,sizeof buf,"1-%u",game.loop);
        if(game_hud_world) lv_label_set_text(game_hud_world,buf);
        game_shown_world=game.loop;
    }
    if(game_shown_time!=game.time) {
        snprintf(buf,sizeof buf,"%s %u",PASSPORT_TEXT_GAME_TIME,game.time);
        if(game_hud_time) lv_label_set_text(game_hud_time,buf);
        game_shown_time=game.time;
    }
    game_overlay_update();
    if(game_field) lv_obj_invalidate(game_field);
}
void passport_ui_game_key(passport_key_t key,passport_key_event_t event) {
    if(!game_page) return;
    /* Sideways grip: UP points left, DOWN points right, OK jumps. */
    bool press=event==PASSPORT_KEY_PRESS;
    if(key==PASSPORT_KEY_UP) passport_game_left(&game,press);
    else if(key==PASSPORT_KEY_DOWN) passport_game_right(&game,press);
    else if(key==PASSPORT_KEY_OK) passport_game_jump(&game,press);
}
void passport_ui_game_reset_input(void) {
    if(game_page) passport_game_reset_input(&game);
}
static const char *const WEEKDAY[]={"MON","TUE","WED","THU","FRI","SAT","SUN"};
static bool build_page(const passport_model_t *m,const passport_record_t *r) {
    const passport_profile_t *p=&r->profile;
    /* Indexed by passport_page_t; badge and picture carry no heading. */
    static const char *titles[]={"","",
        PASSPORT_TEXT_MENU_TITLE,PASSPORT_TEXT_BRIGHTNESS_TITLE,PASSPORT_TEXT_ABOUT_TITLE,"",
        "MUSE AGENT","任务结果","本地笔记","网络与 Muse"};
    const char *heading=*status_text?status_text:
        (m->page<=PASSPORT_PAGE_PICTURE||m->page==PASSPORT_PAGE_GAME?"":titles[m->page]);
    if(m->page==PASSPORT_PAGE_AGENT) heading="";
    /* The game owns the body background; other pages keep it transparent. */
    if(m->page!=PASSPORT_PAGE_GAME && m->page!=PASSPORT_PAGE_AGENT)
        lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
    if(*heading)
        text(body,HEADING_X,HEADING_Y,HEADING_W,HEADING_H,heading,&passport_font_20,*status_text?NEON:INK,true);
    if(m->page==PASSPORT_PAGE_GAME) {
        if(!game_build()) return false;
    } else if(m->page==PASSPORT_PAGE_BADGE) {
        const lv_image_dsc_t *portrait=passport_avatar_current();
        const bool stacked=portrait->header.w>=150;
        avatar=lv_image_create(body);
        if(avatar) {
            lv_obj_set_pos(avatar,stacked?AVATAR_STACK_X:AVATAR_COL_X,stacked?AVATAR_STACK_Y:AVATAR_COL_Y);
            lv_image_set_src(avatar,portrait);
            avatar_scene=passport_scene_by_image(portrait);
            avatar_motion=avatar_scene!=NULL;
            avatar_temporary=!avatar_motion && passport_avatar_is_temporary && portrait==&passport_avatar_frames[0];
            if(avatar_motion) {scene_began=lv_tick_get();passport_animation_show(&animation,scene_began);}
            if(avatar_motion || avatar_temporary) {
                blink_tick=0;avatar_timer=lv_timer_create(animate,avatar_motion?50:100,NULL);
                if(!avatar_timer) page_failed=true;
            }
        }
        else page_failed=true;
        if(avatar_temporary) text(body,TEMP_X,TEMP_Y,TEMP_W,12,"TEMP",&lv_font_montserrat_14,MUTED,true);
        if(stacked) {
            info_text(INFO_STACK_X,INFO_NAME_Y_STACK,INFO_STACK_W,36,field(p->name,PASSPORT_TEXT_NAME_UNSET),&passport_font_24_bold,INK);
            info_text(INFO_STACK_X,INFO_TITLE_Y_STACK,INFO_STACK_W,36,field(p->title,PASSPORT_TEXT_TITLE_UNSET),&passport_font_24_bold,NEON);
            info_text(INFO_STACK_X,INFO_RESEARCH_Y_STACK,INFO_STACK_W,30,field(p->research,PASSPORT_TEXT_RESEARCH_UNSET),&passport_font_20,MUTED);
        } else {
            info_text(INFO_COL_X,INFO_NAME_Y_COL,INFO_COL_W,36,field(p->name,PASSPORT_TEXT_NAME_UNSET),&passport_font_24_bold,INK);
            info_text(INFO_COL_X,INFO_TITLE_Y_COL,INFO_COL_W,36,field(p->title,PASSPORT_TEXT_TITLE_UNSET),&passport_font_24_bold,NEON);
            info_text(INFO_COL_X,INFO_RESEARCH_Y_COL,INFO_COL_W,30,field(p->research,PASSPORT_TEXT_RESEARCH_UNSET),&passport_font_20,MUTED);
        }
    } else if(m->page==PASSPORT_PAGE_PICTURE) {
        const lv_image_dsc_t *picture=passport_image_current();
        if(picture) {
            lv_obj_t *image=lv_image_create(body);
            if(!image) {page_failed=true;}
            else {lv_image_set_src(image,picture);lv_obj_set_pos(image,0,0);}
        } else {
            text(body,CONTENT_X,HINT_TITLE_Y,CONTENT_W,24,PASSPORT_TEXT_IMAGE_UNSET,&passport_font_20,DARK_INK,true);
            text(body,CONTENT_X,HINT_SUB_Y,CONTENT_W,24,PASSPORT_TEXT_IMAGE_HINT,&passport_font_16,DARK_MUTED,true);
        }
    } else if(m->page==PASSPORT_PAGE_MENU) {
        const char *rows[]={PASSPORT_TEXT_MENU_BRIGHTNESS,PASSPORT_TEXT_MENU_ABOUT,
            PASSPORT_TEXT_MENU_CONFIG,PASSPORT_TEXT_MENU_GAME,"任务结果","本地笔记",PASSPORT_TEXT_MENU_RETURN};
        unsigned first=m->menu_index>3?m->menu_index-3:0;
        for(unsigned i=first;i<first+4 && i<7;i++) {
            lv_obj_t *row=panel(body,MENU_X,MENU_Y0+(int)(i-first)*MENU_GAP,MENU_W,MENU_ROW_H,m->menu_index==i?0x123a4a:PANEL,10);
            if(!row) return false;
            if(m->menu_index==i) lv_obj_set_style_border_color(row,lv_color_hex(NEON),0),
                                 lv_obj_set_style_border_width(row,2,0);
            text(row,8,10,MENU_W-16,24,rows[i],&passport_font_20,m->menu_index==i?NEON:INK,true);
        }
    } else if(m->page==PASSPORT_PAGE_BRIGHTNESS) {
        char value[16];snprintf(value,sizeof value,"%u%%",m->brightness);
        text(body,CONTENT_X,BRIGHT_VALUE_Y,CONTENT_W,BRIGHT_VALUE_H,value,&lv_font_montserrat_20,NEON,true);
        for(unsigned i=0;i<3;i++) panel(body,BRIGHT_BAR_X0+(int)i*BRIGHT_BAR_GAP,BRIGHT_BAR_Y,BRIGHT_BAR_W,BRIGHT_BAR_H,m->brightness>=(i+1)*30?NEON:TRACK,4);
        text(body,CONTENT_X,BRIGHT_HINT_Y,CONTENT_W,24,PASSPORT_TEXT_BRIGHTNESS_HINT,&passport_font_16,MUTED,true);
    } else if(m->page==PASSPORT_PAGE_ABOUT) {
        text(body,CONTENT_X,ABOUT_TITLE_Y,CONTENT_W,24,"AI PassportOS",&lv_font_montserrat_20,INK,true);
        text(body,CONTENT_X,ABOUT_VER_Y,CONTENT_W,24,PASSPORT_TEXT_ABOUT_VERSION " " PASSPORT_VERSION,&passport_font_16,NEON,true);
        text(body,CONTENT_X,ABOUT_BOARD_Y,CONTENT_W,24,PASSPORT_TEXT_ABOUT_BOARD,&passport_font_16,MUTED,true);
    } else if(m->page==PASSPORT_PAGE_MUSE_SETUP) {
        const char *info=m->setup_confirm?
            "清除 Muse 配对及网络。\n身份卡、图片和笔记保留。\n重启后需手机重新配对。\n是否继续？":
            setup_web[0]?setup_web:agent_view.connection_info;
        text(body,12,32,216,92,info,&passport_font_16,INK,false);
        const char *normal[]={"打开资料配置页面","重新配对 Muse","返回功能菜单"};
        const char *confirm[]={"取消","确认清除 Muse 配对"};
        unsigned count=m->setup_confirm?2:3;
        for(unsigned i=0;i<count;i++) {
            lv_obj_t *row=panel(body,12,136+(int)i*40,216,34,m->setup_choice==i?0x123a4a:PANEL,8);
            if(!row) return false;
            text(row,8,6,200,24,m->setup_confirm?confirm[i]:normal[i],&passport_font_16,m->setup_choice==i?NEON:INK,true);
        }
        text(body,8,266,224,22,"上下选择  OK 确认  长按返回",&passport_font_16,MUTED,true);
    } else if(m->page==PASSPORT_PAGE_AGENT) {
        if(!agent_build()) return false;
    } else if(m->page>=PASSPORT_PAGE_TASKS) {
        const char *content="",*hint="上下翻阅  长按 OK 返回";
        if(m->page==PASSPORT_PAGE_TASKS) {
            content=agent_view.journal.result[0]?agent_view.journal.result:"暂无任务结果";
            hint="OK 保存短笔记  长按返回";
        } else {
            if(note_index>=agent_view.journal.count) note_index=0;
            content=agent_view.journal.count?agent_view.journal.notes[note_index]:"暂无笔记，可让 Muse 保存短笔记";
            char position[40];snprintf(position,sizeof position,"%u / %u",agent_view.journal.count?note_index+1:0,(unsigned)agent_view.journal.count);
            text(body,12,30,216,22,position,&passport_font_16,NEON,true);
            hint="上下切换笔记  长按返回";
        }
        agent_content=lv_obj_create(body);
        if(!agent_content) {page_failed=true;return false;}
        lv_obj_set_pos(agent_content,12,58);lv_obj_set_size(agent_content,216,154);
        lv_obj_set_style_bg_opa(agent_content,LV_OPA_TRANSP,0);
        lv_obj_set_style_border_width(agent_content,0,0);lv_obj_set_style_pad_all(agent_content,4,0);
        lv_obj_t *label=lv_label_create(agent_content);
        if(!label) {page_failed=true;return false;}
        lv_obj_set_width(label,200);lv_label_set_long_mode(label,LV_LABEL_LONG_MODE_WRAP);
        lv_obj_set_style_text_font(label,&passport_font_16,0);
        lv_obj_set_style_text_color(label,lv_color_hex(INK),0);lv_label_set_text(label,content);
        bool preview=(m->page==PASSPORT_PAGE_AGENT && agent_view.reply[0] && agent_view.state!=PASSPORT_AGENT_CONFIRM) ||
            (m->page==PASSPORT_PAGE_TASKS && agent_view.journal.result[0]);
        text(body,12,216,216,20,preview?"回复预览，完整内容见手机":"",&passport_font_16,MUTED,true);
        text(body,12,238,216,20,agent_view.detail,&passport_font_16,MUTED,true);
        text(body,8,266,224,22,hint,&passport_font_16,NEON,true);
    }
    return page_failed?false:chrome(m);
}
/* Page chrome: the picture page is full-bleed (no status bar), everything else
 * renders inside the body under the status bar. Made explicit so the geometry
 * is the only place this page-level distinction lives. */
static void apply_frame(bool fullscreen) {
    lv_obj_set_pos(body,0,fullscreen?0:BODY_Y);
    lv_obj_set_size(body,BODY_W,fullscreen?SCREEN_H:BODY_H);
    if(fullscreen) {
        lv_obj_set_style_bg_color(body,lv_color_hex(0xffffff),0);
        lv_obj_set_style_bg_opa(body,LV_OPA_COVER,0);
    } else {
        lv_obj_set_style_bg_opa(body,LV_OPA_TRANSP,0);
    }
    lv_obj_t *bar[]={clock_label,battery_label,weekday_label,battery_body,battery_nub,battery_fill};
    for(unsigned i=0;i<6;i++) {
        if(fullscreen) lv_obj_add_flag(bar[i],LV_OBJ_FLAG_HIDDEN);
        else lv_obj_clear_flag(bar[i],LV_OBJ_FLAG_HIDDEN);
    }
}
static void update_battery(int battery) {
    if(battery==last_battery) return;
    last_battery=battery;
    if(battery>=0 && battery<=100) {
        uint32_t bc=battery<20?BAT_RED:battery<50?BAT_YELLOW:NEON;
        lv_obj_set_style_bg_color(battery_fill,lv_color_hex(bc),0);
        lv_obj_set_width(battery_fill,(BAT_FILL_W*battery+50)/100);
    } else lv_obj_set_width(battery_fill,0);
}
bool passport_ui_render(const passport_model_t *m,const passport_record_t *r,int battery,bool valid,int64_t epoch) {
    if(!screen || !m || !r || m->page>PASSPORT_PAGE_MUSE_SETUP) return false;
    /* Status messages are transient: clear them after a short confirmation
     * window so the heading does not stick to the UI forever. */
    if(*status_text && lv_tick_elaps(status_tick)>=3000) {status_text[0]=0;rendered=false;}
    char clock[32]="--:--",level[12]="--%";
    int weekday=-1;
    if(valid) {
        int64_t seconds=(epoch+8*3600)%86400;
        if(seconds<0) seconds+=86400;
        snprintf(clock,sizeof clock,"%02d:%02d",(int)(seconds/3600),(int)(seconds/60%60));
        int64_t days=(epoch+8*3600)/86400;
        if(days<0) days=0;
        weekday=(int)((days%7+4)%7);
        weekday=(weekday+6)%7;
    }
    if(battery>=0 && battery<=100) snprintf(level,sizeof level,"%d%%",battery);
    if(strcmp(clock,last_clock)) {snprintf(last_clock,sizeof last_clock,"%s",clock);lv_label_set_text(clock_label,clock);}
    if(strcmp(level,last_level)) {snprintf(last_level,sizeof last_level,"%s",level);lv_label_set_text(battery_label,level);}
    if(weekday!=last_weekday) {last_weekday=weekday;lv_label_set_text(weekday_label,weekday>=0?WEEKDAY[weekday]:"---");}
    update_battery(battery);
    bool entering=have_model && last_model.page!=PASSPORT_PAGE_BADGE && m->page==PASSPORT_PAGE_BADGE;
    bool focus=have_model && last_model.page==PASSPORT_PAGE_BADGE && m->page==PASSPORT_PAGE_BADGE &&
               m->badge_button!=last_model.badge_button;
    bool acknowledge=pending_ack;pending_ack=false;
    if(!rendered || m->page!=last_model.page ||
       (m->page==PASSPORT_PAGE_MENU && m->menu_index!=last_model.menu_index) ||
       (m->page==PASSPORT_PAGE_MUSE_SETUP && (m->setup_choice!=last_model.setup_choice || m->setup_confirm!=last_model.setup_confirm)) ||
       m->brightness!=last_model.brightness ||
       memcmp(&r->profile,&last_profile,sizeof last_profile)) {
        apply_frame(m->page==PASSPORT_PAGE_PICTURE || m->page==PASSPORT_PAGE_GAME || m->page==PASSPORT_PAGE_AGENT);
        clear_page();page_failed=false;
        if(!build_page(m,r)) {
            clear_page();rendered=false;
            return false;
        }
        last_profile=r->profile;rendered=true;
    }
    if(m->page==PASSPORT_PAGE_BADGE) {
        if(focus) {button_focus(0,m->badge_button==0);button_focus(1,m->badge_button==1);}
        if(avatar_motion && avatar_scene->interactive) {
            if(entering || acknowledge) passport_animation_event(&animation,PASSPORT_ANIMATION_NOD,lv_tick_get());
            if(focus) passport_animation_event(&animation,m->badge_button?PASSPORT_ANIMATION_FOCUS_QR:PASSPORT_ANIMATION_FOCUS_AGENT,lv_tick_get());
            if(entering || acknowledge || focus) animate(NULL);
        }
    }
    last_model=*m;have_model=true;
    if(m->page==PASSPORT_PAGE_AGENT) agent_update();
    if(game_page) game_frame();
    return true;
}
void passport_ui_status(const char *value) {
    snprintf(status_text,sizeof status_text,"%s",value?value:"");
    status_tick=lv_tick_get();
    rendered=false;
}
void passport_ui_acknowledge(void) {
    /* Do not queue stale success reactions while a different page is visible. */
    if(avatar && avatar_motion) pending_ack=true;
}
void passport_ui_agent_snapshot(const passport_agent_model_t *value) {
    if(!value || value->revision==agent_view.revision) return;
    if(value->state!=agent_view.state || value->voice!=agent_view.voice) agent_visual_since=lv_tick_get();
    if(value->generation!=agent_view.generation && agent_stage && agent_content) lv_obj_scroll_to_y(agent_content,0,LV_ANIM_OFF);
    agent_view=*value;
    agent_dirty=true;
    if(have_model && last_model.page>PASSPORT_PAGE_AGENT) {agent_content=NULL;rendered=false;}
}
void passport_ui_agent_scroll(int direction) {
    if(!have_model || last_model.page<PASSPORT_PAGE_AGENT) return;
    if(last_model.page==PASSPORT_PAGE_NOTES && agent_view.journal.count) {
        unsigned count=agent_view.journal.count;
        note_index=(note_index+(direction>0?1:count-1))%count;
        agent_content=NULL;rendered=false;
    } else if(agent_content) lv_obj_scroll_by(agent_content,0,direction>0?-96:96,LV_ANIM_OFF);
}
void passport_ui_configuration(const char *address,const char *password) {
    if(address && password) snprintf(setup_web,sizeof setup_web,"http://%.15s\n用户：passport\n访问码：%.12s",address,password);
    else setup_web[0]=0;
    if(have_model && last_model.page==PASSPORT_PAGE_MUSE_SETUP) rendered=false;
}
size_t passport_ui_page_buffers(void) {return 0;}
void passport_ui_destroy(void) {
    clear_page();
    if(screen) {
        if(lv_screen_active()==screen && previous_screen) lv_screen_load(previous_screen);
        lv_obj_delete(screen);
    }
    screen=body=clock_label=battery_label=weekday_label=battery_body=battery_fill=battery_nub=NULL;
    rendered=false;have_model=false;pending_ack=false;
}
