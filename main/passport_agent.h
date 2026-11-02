#pragma once
#include "passport_agent_model.h"
#include "esp_err.h"

/* Every transport call is made by the one audio/agent worker. */
typedef struct {
    passport_agent_state_t (*connection)(void);
    bool (*confirm)(uint32_t displayed_request);
    bool (*begin)(void);
    void (*audio)(const int16_t *,size_t);
    void (*end)(void);
    void (*cancel)(void);
    /* 0 none, 1 transcript, 2 reply, 3 done, 4 error. */
    int (*event)(char *,size_t);
    uint32_t (*confirmation)(void);
    void (*repair)(void);
    void (*describe)(char *,size_t);
} passport_agent_transport_t;

enum { PASSPORT_AGENT_TOGGLE, PASSPORT_AGENT_LEAVE, PASSPORT_AGENT_SAVE_RESULT,
       PASSPORT_AGENT_SAVE_TEXT, PASSPORT_AGENT_REPAIR };
esp_err_t passport_agent_start(const passport_agent_transport_t *);
esp_err_t passport_agent_stop(void);
bool passport_agent_action(unsigned action,const char *text);
bool passport_agent_confirm_pairing(uint32_t displayed_request);
bool passport_agent_snapshot(passport_agent_model_t *);
/* Page visibility controls autoplay; safe from the UI owner without LVGL. */
void passport_agent_set_visible(bool);
/* Default firmware has no transport; the Muse build supplies the strong hook. */
const passport_agent_transport_t *passport_agent_transport(void);
