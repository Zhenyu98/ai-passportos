#pragma once
#include "passport_protocol.h"
#include "esp_err.h"

/* One lifecycle owner. The service must not retain request/reply pointers.
 * It may synchronously wait for its own bounded storage worker.
 * stop retains live resources on timeout; never delete dependent state then. */
esp_err_t passport_usb_start(passport_service_fn service, void *user);
esp_err_t passport_usb_stop(void);
