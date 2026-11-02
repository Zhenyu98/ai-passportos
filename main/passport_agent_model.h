#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "passport_speech.h"

#define PASSPORT_AGENT_REPLY_MAX 1024
#define PASSPORT_AGENT_NOTE_MAX 256
#define PASSPORT_AGENT_NOTES 8
#define PASSPORT_AGENT_RECORD_MS 30000

typedef enum {
    PASSPORT_AGENT_UNAVAILABLE, PASSPORT_AGENT_UNPAIRED, PASSPORT_AGENT_CONFIRM,
    PASSPORT_AGENT_CONNECTING, PASSPORT_AGENT_READY, PASSPORT_AGENT_OFFLINE,
    PASSPORT_AGENT_RECORDING, PASSPORT_AGENT_WAITING, PASSPORT_AGENT_REPLY,
    PASSPORT_AGENT_ERROR
} passport_agent_state_t;

typedef struct {
    uint32_t version;
    uint32_t count;
    char result[PASSPORT_AGENT_REPLY_MAX];
    char notes[PASSPORT_AGENT_NOTES][PASSPORT_AGENT_NOTE_MAX];
} passport_agent_journal_t;

typedef struct {
    passport_agent_state_t state, connection;
    uint32_t generation, revision, confirmation;
    bool truncated;
    passport_voice_state_t voice;
    uint8_t level;
    char voice_detail[64];
    char heard[PASSPORT_AGENT_NOTE_MAX];
    char reply[PASSPORT_AGENT_REPLY_MAX];
    char detail[96];
    char connection_info[128];
    passport_agent_journal_t journal;
} passport_agent_model_t;

void passport_agent_model_init(passport_agent_model_t *);
void passport_agent_connection(passport_agent_model_t *,passport_agent_state_t);
uint32_t passport_agent_begin(passport_agent_model_t *);
void passport_agent_sent(passport_agent_model_t *);
void passport_agent_leave(passport_agent_model_t *);
void passport_agent_event(passport_agent_model_t *,uint32_t,passport_agent_state_t,const char *);
bool passport_agent_copy(char *,size_t,const char *);
bool passport_agent_save_note(passport_agent_model_t *,const char *);
bool passport_agent_restore(passport_agent_model_t *,const void *,size_t);
