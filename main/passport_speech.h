#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define PASSPORT_SPEECH_RATE 16000u
#define PASSPORT_SPEECH_SECONDS 120u
#define PASSPORT_SPEECH_TEXT_MAX 1024u
typedef enum {PASSPORT_SPEECH_OK,PASSPORT_SPEECH_FAILED,PASSPORT_SPEECH_CANCELLED} passport_speech_result_t;
typedef enum {PASSPORT_VOICE_UNCONFIGURED,PASSPORT_VOICE_READY,PASSPORT_VOICE_PREPARING,
              PASSPORT_VOICE_PLAYING,PASSPORT_VOICE_DONE,PASSPORT_VOICE_STOPPED,PASSPORT_VOICE_FAILED} passport_voice_state_t;
typedef struct {char url[224];char token[129];} passport_speech_config_t;
typedef struct {
    void *context;
    bool (*open)(void *,const char *text);
    /* PCM16LE mono 16 kHz. 0 EOF, -1 error, -2 retry (bounded by now_ms). */
    int (*read)(void *,uint8_t *,size_t);
    void (*close)(void *);
    bool (*begin)(void *);
    bool (*write)(void *,const int16_t *,size_t frames);
    bool (*cancelled)(void *);
    uint32_t (*now_ms)(void *);
} passport_speech_io_t;
passport_speech_result_t passport_speech_play(const passport_speech_io_t *,const char *);
bool passport_speech_config_valid(const passport_speech_config_t *);
/* Native adapter: called only by the audio owner; cancellation must be atomic. */
bool passport_speech_configured(void);
passport_speech_result_t passport_speech_http(const char *,const passport_speech_io_t *sink);
bool passport_speech_configuration_line(const char *,size_t);
