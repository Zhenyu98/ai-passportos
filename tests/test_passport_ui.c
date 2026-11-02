#include "passport_ui.h"
#include "passport_assets.h"
#include "passport_qr.h"
#include "passport_strings.h"
#include "src/display/lv_display_private.h"
#include "src/widgets/label/lv_label_private.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GAME_DEMO_STEPS 90
#define GAME_DEMO_JUMP 25
static uint16_t pixels[240*320];
static uint16_t draw[240*40];
static uint16_t picture_pixels[240*320];
static lv_image_dsc_t test_picture;
static bool picture_present;
static bool static_avatar;
static lv_image_dsc_t uploaded_avatar;
static const passport_motion_scene_t *forced_scene;
const lv_image_dsc_t *__real_passport_avatar_current(void);
const lv_image_dsc_t *__wrap_passport_avatar_current(void) {
    return forced_scene?forced_scene->frames:static_avatar?&uploaded_avatar:__real_passport_avatar_current();
}
void passport_ui_acknowledge(void);
const lv_image_dsc_t *__real_passport_image_current(void);
const lv_image_dsc_t *__wrap_passport_image_current(void) {
    return picture_present?&test_picture:__real_passport_image_current();
}
static lv_obj_t *fail_container_parent;
lv_obj_t *__real_lv_obj_create(lv_obj_t *parent);
lv_obj_t *__wrap_lv_obj_create(lv_obj_t *parent) {
    if(parent && parent==fail_container_parent) {fail_container_parent=NULL;return NULL;}
    return __real_lv_obj_create(parent);
}
static void flush(lv_display_t *display,const lv_area_t *area,uint8_t *data) {
    for (int y=area->y1;y<=area->y2;y++) {
        memcpy(pixels+y*240+area->x1,data,(area->x2-area->x1+1)*2);
        data+=(area->x2-area->x1+1)*2;
    }
    lv_display_flush_ready(display);
}
static void dump(const char *directory,const char *name) {
    char path[1024]; snprintf(path,sizeof path,"%s/%s.rgb565",directory,name);
    FILE *f=fopen(path,"wb"); assert(f); assert(fwrite(pixels,2,240*320,f)==240*320); fclose(f);
}
static bool has_text(lv_obj_t *root,const char *needle) {
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),needle))return true;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++)
        if(has_text(lv_obj_get_child(root,i),needle))return true;
    return false;
}
static bool has_fragment(lv_obj_t *root,const char *needle) {
    if(lv_obj_check_type(root,&lv_label_class) && strstr(lv_label_get_text(root),needle)) return true;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++)
        if(has_fragment(lv_obj_get_child(root,i),needle)) return true;
    return false;
}
static void check_agent_canvas(void) {
    lv_obj_t *root=lv_screen_active(),*canvas=lv_obj_get_child(root,2);
    assert(lv_obj_get_width(canvas)==240 && lv_obj_get_height(canvas)==320);
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++)
        if(i!=2) assert(lv_obj_has_flag(lv_obj_get_child(root,i),LV_OBJ_FLAG_HIDDEN));
    assert(pixels[0]==0xffff && pixels[239]==0xffff && pixels[319*240]==0xffff && pixels[320*240-1]==0xffff);
    assert(has_text(root,passport_agent_name));
    assert(!has_fragment(root,"· MUSE") && !has_fragment(root,"长按"));
}
static lv_obj_t *find_avatar(lv_obj_t *root) {
    if(lv_obj_check_type(root,&lv_image_class)) return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *found=find_avatar(lv_obj_get_child(root,i));
        if(found) return found;
    }
    return NULL;
}
static unsigned timer_count(void) {
    unsigned count=0;
    for(lv_timer_t *timer=lv_timer_get_next(NULL);timer;timer=lv_timer_get_next(timer)) count++;
    return count;
}
static lv_obj_t *find_label(lv_obj_t *root,const char *value) {
    if(lv_obj_check_type(root,&lv_label_class) && !strcmp(lv_label_get_text(root),value)) return root;
    for(unsigned i=0;i<lv_obj_get_child_count(root);i++) {
        lv_obj_t *found=find_label(lv_obj_get_child(root,i),value);
        if(found) return found;
    }
    return NULL;
}
/* Catch short text moving vertically when its box is shorter than the font,
 * and long text wrapping or scrolling vertically instead of horizontally. */
static void check_info_overflow(lv_display_t *display,const char *directory) {
    passport_model_t model;passport_model_init(&model,60);
    passport_record_t record={.schema=1,.brightness=60};
    static uint16_t wide_pixels[216*112];
    for(unsigned wide=0;wide<2;wide++) {
        const passport_motion_scene_t *scene=passport_scene_by_id(wide?2:1);
        uploaded_avatar=scene?scene->frames[0]:passport_avatar_frames[0];
        if(wide && !scene) uploaded_avatar=(lv_image_dsc_t){
                .header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,.w=216,.h=112,.stride=432},
                .data_size=sizeof wide_pixels,.data=(const uint8_t *)wide_pixels};
        assert((uploaded_avatar.header.w>=150)==(wide!=0));
        static_avatar=true;passport_ui_status("");
        strcpy(record.profile.name,"A");strcpy(record.profile.title,"B");strcpy(record.profile.research,"C");
        assert(passport_ui_render(&model,&record,-1,false,0));lv_refr_now(display);
        const char *short_text[]={"A","B","C"};
        for(unsigned tick=0;tick<40;tick++) {
            lv_tick_inc(100);lv_timer_handler();lv_refr_now(display);
            for(unsigned i=0;i<3;i++) {
                lv_obj_t *obj=find_label(lv_screen_active(),short_text[i]);assert(obj);
                lv_label_t *label=(lv_label_t *)obj;
                assert(label->offset.x==0 && label->offset.y==0);
                assert(lv_obj_get_content_height(obj)>=lv_font_get_line_height(lv_obj_get_style_text_font(obj,0)));
            }
        }
        char snapshot[48];snprintf(snapshot,sizeof snapshot,"text-short-%u",wide);dump(directory,snapshot);
        strcpy(record.profile.name,"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM");
        strcpy(record.profile.title,"WWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWWW");
        strcpy(record.profile.research,"MMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMMM research");
        assert(passport_ui_render(&model,&record,-1,false,0));lv_refr_now(display);
        const char *long_text[]={record.profile.name,record.profile.title,record.profile.research};
        bool moved[3]={false,false,false};
        bool reversed[3]={false,false,false};
        int previous_x[3]={0,0,0};
        for(unsigned tick=0;tick<600;tick++) {
            lv_tick_inc(100);lv_timer_handler();lv_refr_now(display);
            for(unsigned i=0;i<3;i++) {
                lv_obj_t *obj=find_label(lv_screen_active(),long_text[i]);assert(obj);
                lv_label_t *label=(lv_label_t *)obj;
                assert(label->offset.y==0);
                moved[i]|=label->offset.x<0;
                reversed[i]|=moved[i] && label->offset.x>previous_x[i];
                previous_x[i]=label->offset.x;
            }
            if(tick==39) {
                snprintf(snapshot,sizeof snapshot,"text-long-%u",wide);dump(directory,snapshot);
            }
        }
        assert(moved[0] && moved[1] && moved[2]);
        assert(reversed[0] && reversed[1] && reversed[2]);
        /* Shrinking content stops existing motion; multiline profile data is
         * shown as one line without changing the saved value. */
        strcpy(record.profile.name,"A");strcpy(record.profile.title,"B");strcpy(record.profile.research,"C\nD");
        assert(passport_ui_render(&model,&record,-1,false,0));lv_refr_now(display);
        assert(!strcmp(record.profile.research,"C\nD"));
        lv_obj_t *single=find_label(lv_screen_active(),"C D");assert(single);
        for(unsigned tick=0;tick<20;tick++) {
            lv_tick_inc(100);lv_timer_handler();
            assert(((lv_label_t *)single)->offset.x==0 && ((lv_label_t *)single)->offset.y==0);
        }
    }
    static_avatar=false;
}
int main(int argc,char **argv) {
    assert(argc==2 || argc==3);
    lv_init();
    const char *required=getenv("PASSPORT_PREVIEW_REQUIRE_MOTION_SCENES");
    assert(!required || passport_motion_scene_count>=strtoul(required,NULL,10));
    lv_display_t *display=lv_display_create(240,320); assert(display);
    lv_display_set_color_format(display,LV_COLOR_FORMAT_RGB565);
    lv_display_set_flush_cb(display,flush);
    lv_display_set_buffers(display,draw,NULL,sizeof draw,LV_DISPLAY_RENDER_MODE_PARTIAL);
    for (size_t i=0;i<passport_asset_glyph_count;i++) {
        lv_font_glyph_dsc_t glyph;
        assert(lv_font_get_glyph_dsc(&passport_font_16,&glyph,passport_asset_glyphs[i],0));
        assert(!glyph.is_placeholder);
        assert(lv_font_get_glyph_dsc(&passport_font_20,&glyph,passport_asset_glyphs[i],0));
        assert(!glyph.is_placeholder);
        assert(lv_font_get_glyph_dsc(&passport_font_24_bold,&glyph,passport_asset_glyphs[i],0));
        assert(!glyph.is_placeholder);
    }
    passport_model_t model; passport_model_init(&model,60);
    passport_record_t record={.schema=1,.brightness=60};
    if (argc==3) {
        FILE *f=fopen(argv[2],"rb"); assert(f);
        char *fields[]={record.profile.name,record.profile.title,record.profile.research,record.profile.bio};
        size_t caps[]={sizeof record.profile.name,sizeof record.profile.title,sizeof record.profile.research,sizeof record.profile.bio};
        for(unsigned i=0;i<4;i++) { assert(fgets(fields[i],(int)caps[i],f)); fields[i][strcspn(fields[i],"\r\n")]=0; }
        fclose(f);
    }
    assert(passport_ui_create());
    assert(passport_ui_render(&model,&record,-1,false,0)); lv_refr_now(display); dump(argv[1],"badge");
    lv_obj_t *original_avatar=find_avatar(lv_screen_active()); assert(original_avatar);
    unsigned badge_timers=timer_count();
    model.badge_button=1;
    assert(passport_ui_render(&model,&record,-1,false,0));
    assert(find_avatar(lv_screen_active())==original_avatar);
    if(passport_motion_count==18) {
        assert(lv_image_get_src(original_avatar)==&passport_motion_frames[10]);
        lv_tick_inc(100);lv_timer_handler();
        assert(lv_image_get_src(original_avatar)==&passport_motion_frames[11]);
        lv_refr_now(display);dump(argv[1],"motion-focus-qr");
    }
    for(unsigned press=0;press<100;press++) {
        model.badge_button^=1u;
        assert(passport_ui_render(&model,&record,-1,false,0));
        assert(find_avatar(lv_screen_active())==original_avatar);
        assert(timer_count()==badge_timers);
    }
    passport_ui_acknowledge();
    assert(passport_ui_render(&model,&record,-1,false,0));
    if(passport_motion_count==18) {
        assert(lv_image_get_src(original_avatar)==&passport_motion_frames[13]);
        for(unsigned frame=0;frame<12;frame++) {
            lv_tick_inc(50);lv_timer_handler();lv_refr_now(display);
            char name[32];snprintf(name,sizeof name,"motion-nod-%02u",frame);dump(argv[1],name);
        }
        assert(lv_image_get_src(original_avatar)==&passport_motion_frames[0]);
        assert(lv_obj_get_style_img_opa(original_avatar,0)==LV_OPA_COVER);
    }
    model.page=PASSPORT_PAGE_MENU;
    assert(passport_ui_render(&model,&record,-1,false,0));
    assert(find_avatar(lv_screen_active())==NULL);
    assert(timer_count()+1==badge_timers);
    lv_tick_inc(4000);lv_timer_handler();
    model.page=PASSPORT_PAGE_BADGE;
    assert(passport_ui_render(&model,&record,-1,false,0));
    if(passport_motion_count==18)
        assert(lv_image_get_src(find_avatar(lv_screen_active()))==&passport_motion_frames[13]);
    uploaded_avatar=passport_avatar_frames[0];
    static_avatar=true;passport_ui_status("");
    assert(passport_ui_render(&model,&record,-1,false,0));
    lv_obj_t *uploaded_image=find_avatar(lv_screen_active());
    assert(lv_image_get_src(uploaded_image)==&uploaded_avatar);
    unsigned static_timers=timer_count();
    assert(static_timers+1==badge_timers);
    lv_refr_now(display); /* Resolve the new layout before sampling coordinates. */
    int static_y=lv_obj_get_y(uploaded_image);
    for(unsigned tick=0;tick<50;tick++) {
        lv_tick_inc(100);lv_timer_handler();
        assert(lv_image_get_src(uploaded_image)==&uploaded_avatar);
        assert(lv_obj_get_y(uploaded_image)==static_y);
        assert(lv_obj_get_style_img_opa(uploaded_image,0)==LV_OPA_COVER);
        assert(timer_count()==static_timers);
    }
    static_avatar=false;passport_ui_status("");
    assert(passport_ui_render(&model,&record,-1,false,0));
    for(unsigned scene_id=2;scene_id<=3;scene_id++) {
        forced_scene=passport_scene_by_id(scene_id);
        if(!forced_scene) continue; /* Public builds intentionally omit personal art. */
        passport_ui_status("");model.page=PASSPORT_PAGE_BADGE;
        assert(passport_ui_render(&model,&record,-1,false,0));
        lv_obj_t *scene_image=find_avatar(lv_screen_active());assert(scene_image);
        assert(lv_image_get_src(scene_image)==&forced_scene->frames[0]);
        lv_refr_now(display);
        char label[48];snprintf(label,sizeof label,"scene-%u-frame-0",scene_id);dump(argv[1],label);
        lv_tick_inc(forced_scene->durations[0]+1);lv_timer_handler();lv_refr_now(display);
        assert(lv_image_get_src(scene_image)==&forced_scene->frames[1]);
        snprintf(label,sizeof label,"scene-%u-frame-1",scene_id);dump(argv[1],label);
        for(unsigned n=2;n<6;n++) {
            lv_tick_inc(forced_scene->durations[n-1]);lv_timer_handler();lv_refr_now(display);
            assert(lv_image_get_src(scene_image)==&forced_scene->frames[n]);
            snprintf(label,sizeof label,"scene-%u-frame-%u",scene_id,n);dump(argv[1],label);
        }
        model.badge_button^=1;
        assert(passport_ui_render(&model,&record,-1,false,0));
        assert(find_avatar(lv_screen_active())==scene_image);
        assert(lv_image_get_src(scene_image)==&forced_scene->frames[5]);
        model.page=PASSPORT_PAGE_GAME;
        assert(passport_ui_render(&model,&record,-1,false,0));
        assert(!find_avatar(lv_screen_active()));
        assert(timer_count()+1==badge_timers);
        lv_tick_inc(100);lv_timer_handler();
        model.page=PASSPORT_PAGE_BADGE;
        assert(passport_ui_render(&model,&record,-1,false,0));
        assert(lv_image_get_src(find_avatar(lv_screen_active()))==&forced_scene->frames[0]);
    }
    forced_scene=NULL;passport_ui_status("");
    assert(passport_ui_render(&model,&record,-1,false,0));
    passport_ui_status("Saved");
    assert(passport_ui_render(&model,&record,-1,false,0));
    assert(has_text(lv_screen_active(),"Saved"));
    lv_tick_inc(2999);assert(passport_ui_render(&model,&record,-1,false,0));
    assert(has_text(lv_screen_active(),"Saved"));
    lv_tick_inc(1);assert(passport_ui_render(&model,&record,-1,false,0));
    assert(!has_text(lv_screen_active(),"Saved"));
    unsigned screens=display->screen_cnt;
    strcpy(record.profile.homepage,"https://example.org/a");
    passport_qr_t qr;assert(passport_qr_encode(record.profile.homepage,&qr,240));
    for(int y=0;y<320;y++) for(int x=0;x<240;x++)
        picture_pixels[y*240+x]=passport_qr_dark_pixel(&qr,x,y-40)?0:0xffff;
    test_picture=(lv_image_dsc_t){.header={.magic=LV_IMAGE_HEADER_MAGIC,.cf=LV_COLOR_FORMAT_RGB565,
        .w=240,.h=320,.stride=480},.data_size=sizeof picture_pixels,.data=(const uint8_t *)picture_pixels};
    picture_present=true;model.page=PASSPORT_PAGE_PICTURE;
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    lv_refr_now(display);dump(argv[1],"homepage");
    for(int page=PASSPORT_PAGE_MENU;page<=PASSPORT_PAGE_MENU;page++) {
        model.page=(passport_page_t)page;
        fail_container_parent=lv_obj_get_child(lv_screen_active(),2);
        assert(!passport_ui_render(&model,&record,-1,false,0));
        assert(display->screen_cnt==screens && passport_ui_page_buffers()==0);
    }
    model.page=PASSPORT_PAGE_BADGE;
    assert(passport_ui_render(&model,&record,75,true,1761004800)); lv_refr_now(display); dump(argv[1],"badge-live");
    const char *name=record.profile.name[0]?record.profile.name:"姓名未设置";
    const char *organization=record.profile.title[0]?record.profile.title:PASSPORT_TEXT_TITLE_UNSET;
    lv_obj_t *name_label=find_label(lv_screen_active(),name),*organization_label=find_label(lv_screen_active(),organization);
    assert(name_label && organization_label);
    assert(lv_obj_get_style_text_font(name_label,0)==&passport_font_24_bold);
    assert(lv_obj_get_style_text_font(organization_label,0)==&passport_font_24_bold);
    lv_obj_t *agent_button=find_label(lv_screen_active(),"AGENT");assert(agent_button);
    assert(lv_obj_get_height(lv_obj_get_parent(agent_button))<=48);
    lv_obj_t *shown_clock=find_label(lv_screen_active(),"08:00");assert(shown_clock);
    lv_point_t clock_size;
    lv_text_get_size(&clock_size,"08:00",lv_obj_get_style_text_font(shown_clock,0),0,0,LV_COORD_MAX,LV_TEXT_FLAG_EXPAND);
    assert(clock_size.x<=lv_obj_get_content_width(shown_clock));
    size_t stable_free=0,stable_blocks=0;
    passport_agent_model_t agent={.revision=1,.state=PASSPORT_AGENT_REPLY,
        .connection=PASSPORT_AGENT_READY,.voice=PASSPORT_VOICE_READY,.journal={.version=1,.count=1}};
    strcpy(agent.reply,"明早 9:30 产品评审，已帮你记下。");
    strcpy(agent.journal.result,agent.reply);
    strcpy(agent.journal.notes[0],"会议笔记：周五前整理需求清单。");
    passport_ui_agent_snapshot(&agent);
    model.page=PASSPORT_PAGE_AGENT;
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),agent.reply));
    assert(has_text(lv_screen_active(),"回复预览 · 完整见手机"));
    lv_refr_now(display);check_agent_canvas();dump(argv[1],"agent-reply");
    lv_obj_t *agent_avatar=find_avatar(lv_screen_active());assert(agent_avatar);
    assert(lv_image_get_src(agent_avatar)==passport_agent_avatar_frame(0));
    /* Public builds carry motion scenes but keep the personal agent art private. */
    const char *agent_name=getenv("PASSPORT_PREVIEW_REQUIRE_AGENT_NAME");
    if(agent_name) assert(!strcmp(passport_agent_name,agent_name));
    unsigned agent_timers=timer_count();
    for(unsigned i=0;i<20;i++) strcpy(agent.reply+i*strlen("今日待办记录与结果。\n"),"今日待办记录与结果。\n");
    agent.revision++;passport_ui_agent_snapshot(&agent);
    assert(passport_ui_render(&model,&record,75,true,1761004800));lv_refr_now(display);
    lv_obj_t *reply_label=find_label(lv_screen_active(),agent.reply);assert(reply_label);
    lv_obj_t *reply_container=lv_obj_get_parent(reply_label);
    lv_obj_scroll_to_y(reply_container,80,LV_ANIM_OFF);lv_refr_now(display);
    int32_t reply_scroll=lv_obj_get_scroll_y(reply_container);assert(reply_scroll>0);
    for(unsigned i=0;i<80;i++) {
        agent.level=(uint8_t)(i%100);agent.revision++;passport_ui_agent_snapshot(&agent);
        assert(passport_ui_render(&model,&record,75,true,1761004800));
        lv_tick_inc(50);lv_timer_handler();lv_refr_now(display);
        assert(find_avatar(lv_screen_active())==agent_avatar);
        assert(find_label(lv_screen_active(),agent.reply)==reply_label);
        assert(timer_count()==agent_timers);
        assert(lv_obj_get_scroll_y(reply_container)==reply_scroll);
    }
    strcpy(agent.reply,"明早 9:30 产品评审，已帮你记下。");agent.revision++;passport_ui_agent_snapshot(&agent);
    agent.voice=PASSPORT_VOICE_PLAYING;agent.level=70;agent.revision++;passport_ui_agent_snapshot(&agent);
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"按 OK 停止朗读"));
    lv_obj_update_layout(reply_container);
    assert(lv_obj_get_scroll_y(reply_container)==0);
    lv_refr_now(display);check_agent_canvas();dump(argv[1],"agent-speaking");
    for(unsigned frame=0;frame<12;frame++) {
        agent.level=(uint8_t)(45+frame%4*15);agent.revision++;passport_ui_agent_snapshot(&agent);
        assert(passport_ui_render(&model,&record,75,true,1761004800));
        lv_tick_inc(150);lv_timer_handler();lv_refr_now(display);
        char name[40];snprintf(name,sizeof name,"agent-speaking-frame-%02u",frame);dump(argv[1],name);
    }
    agent.voice=PASSPORT_VOICE_PREPARING;agent.revision++;passport_ui_agent_snapshot(&agent);
    assert(passport_ui_render(&model,&record,75,true,1761004800));lv_refr_now(display);dump(argv[1],"agent-voice-preparing");
    agent.voice=PASSPORT_VOICE_FAILED;strcpy(agent.voice_detail,"朗读失败，文字已保留");agent.revision++;passport_ui_agent_snapshot(&agent);
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),agent.reply));assert(has_text(lv_screen_active(),agent.voice_detail));
    lv_refr_now(display);dump(argv[1],"agent-voice-failed");
    agent.state=PASSPORT_AGENT_ERROR;strcpy(agent.detail,"麦克风不可用");agent.revision++;
    passport_ui_agent_snapshot(&agent);assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),agent.detail));
    assert(!has_text(lv_screen_active(),agent.voice_detail));
    agent.state=PASSPORT_AGENT_REPLY;agent.voice=PASSPORT_VOICE_READY;
    agent.detail[0]=agent.voice_detail[0]=0;agent.connection=PASSPORT_AGENT_OFFLINE;agent.revision++;
    passport_ui_agent_snapshot(&agent);assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),agent.reply));assert(has_text(lv_screen_active(),"网络已断开"));
    assert(!has_text(lv_screen_active(),"按 OK 说话"));
    lv_refr_now(display);check_agent_canvas();dump(argv[1],"agent-offline");
    agent.connection=PASSPORT_AGENT_READY;agent.revision++;
    passport_ui_agent_snapshot(&agent);assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(!has_text(lv_screen_active(),"网络已断开") && has_text(lv_screen_active(),"按 OK 说话"));
    agent.voice=PASSPORT_VOICE_UNCONFIGURED;agent.revision++;
    passport_ui_agent_snapshot(&agent);assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"仅文字回复 · 完整见手机"));
    agent.state=PASSPORT_AGENT_WAITING;agent.voice=PASSPORT_VOICE_READY;agent.voice_detail[0]=0;agent.revision++;
    passport_ui_agent_snapshot(&agent);assert(passport_ui_render(&model,&record,75,true,1761004800));
    lv_refr_now(display);dump(argv[1],"agent-thinking");
    agent.state=PASSPORT_AGENT_RECORDING;agent.reply[0]=0;agent.revision++;
    passport_ui_agent_snapshot(&agent);
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"按 OK 发送"));
    lv_refr_now(display);check_agent_canvas();dump(argv[1],"agent-recording");
    agent.state=PASSPORT_AGENT_CONFIRM;agent.revision++;
    passport_ui_agent_snapshot(&agent);
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"按 OK 确认配对"));
    lv_refr_now(display);check_agent_canvas();dump(argv[1],"agent-pairing");
    lv_obj_t *consent=find_label(lv_screen_active(),"确认是你发起的配对，\n再按 OK 允许连接。");assert(consent);
    assert(lv_obj_get_height(consent)<=lv_obj_get_content_height(lv_obj_get_parent(consent)));
    agent.state=PASSPORT_AGENT_READY;agent.connection=PASSPORT_AGENT_READY;agent.voice=PASSPORT_VOICE_READY;
    agent.reply[0]=agent.heard[0]=agent.detail[0]=agent.voice_detail[0]=0;agent.revision++;
    passport_ui_agent_snapshot(&agent);assert(passport_ui_render(&model,&record,75,true,1761004800));
    lv_refr_now(display);check_agent_canvas();dump(argv[1],"agent-idle");
    assert(has_text(lv_screen_active(),"按 OK 说话"));
    assert(!has_fragment(lv_screen_active(),"就绪") && !has_fragment(lv_screen_active(),"交办任务"));
    model.page=PASSPORT_PAGE_BADGE;
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(!lv_obj_has_flag(lv_obj_get_child(lv_screen_active(),0),LV_OBJ_FLAG_HIDDEN));
    model.page=PASSPORT_PAGE_MUSE_SETUP;model.setup_choice=0;model.setup_confirm=false;
    passport_ui_configuration("192.168.1.20","0123456789ab");
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"http://192.168.1.20\n用户：passport\n访问码：0123456789ab"));
    lv_refr_now(display);dump(argv[1],"muse-setup");
    model.setup_confirm=true;
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"确认清除 Muse 配对"));
    lv_refr_now(display);dump(argv[1],"muse-repair-confirm");
    model.setup_confirm=false;passport_ui_configuration(NULL,NULL);
    /* Offline Muse builds fall back to the badge's own hotspot. */
    passport_ui_hotspot("Badge-1A2B");
    assert(passport_ui_render(&model,&record,75,true,1761004800));
    assert(has_text(lv_screen_active(),"热点：Badge-1A2B\n手机连上后自动弹出\n或打开 192.168.4.1"));
    lv_refr_now(display);dump(argv[1],"muse-hotspot");
    passport_ui_configuration(NULL,NULL);
    /* A short scripted run documents the in-game view, not just its title overlay. */
    model.page=PASSPORT_PAGE_GAME;
    assert(passport_ui_render(&model,&record,-1,false,0));
    passport_ui_game_key(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS);passport_ui_game_key(PASSPORT_KEY_OK,PASSPORT_KEY_RELEASE);
    passport_ui_game_key(PASSPORT_KEY_DOWN,PASSPORT_KEY_PRESS);
    for(unsigned step=0;step<GAME_DEMO_STEPS;step++) {
        if(step%GAME_DEMO_JUMP==GAME_DEMO_JUMP-1) passport_ui_game_key(PASSPORT_KEY_OK,PASSPORT_KEY_PRESS);
        if(step%GAME_DEMO_JUMP==0) passport_ui_game_key(PASSPORT_KEY_OK,PASSPORT_KEY_RELEASE);
        lv_tick_inc(33);assert(passport_ui_render(&model,&record,-1,false,0));
    }
    passport_ui_game_key(PASSPORT_KEY_DOWN,PASSPORT_KEY_RELEASE);
    lv_refr_now(display);dump(argv[1],"game-play");
    for(unsigned iteration=0;iteration<100;iteration++) {
        for(int page=PASSPORT_PAGE_BADGE;page<=PASSPORT_PAGE_MUSE_SETUP;page++) {
            model.page=(passport_page_t)page;
            assert(passport_ui_render(&model,&record,-1,false,0));
            lv_tick_inc(100); lv_timer_handler();
            if(iteration==0) { char name[32]; snprintf(name,sizeof name,"page-%d",page); dump(argv[1],name); }
            assert(passport_ui_page_buffers()==0);
        }
        lv_refr_now(display); /* Compare completed frames, not queued draw work. */
        lv_mem_monitor_t memory;lv_mem_monitor(&memory);
        if(iteration==10) {stable_free=memory.free_size;stable_blocks=memory.used_cnt;}
        if(iteration>10) {
            /* TLSF split/coalescing changes free-block overhead by 56 bytes in
             * the measured cycle. Live allocations must stay exactly constant. */
            assert(memory.used_cnt==stable_blocks);
            assert(memory.free_size+128>=stable_free);
            assert(memory.free_biggest_size>=8000);
        }
    }
    check_info_overflow(display,argv[1]);
    passport_ui_destroy(); assert(passport_ui_page_buffers()==0);
    assert(passport_ui_create()); passport_ui_destroy();
    lv_display_delete(display); lv_deinit();
    printf("Actual LVGL UI, %zu glyphs, %d page transitions and teardown: PASS\n",
           passport_asset_glyph_count,100*(PASSPORT_PAGE_MUSE_SETUP+1));
    return 0;
}
