#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef struct {
    uint8_t bytes[512];
    int modules,scale,origin,canvas;
} passport_qr_t;
bool passport_qr_encode(const char *url,passport_qr_t *out,int canvas);
bool passport_qr_module(const passport_qr_t *,int x,int y);
bool passport_qr_dark_pixel(const passport_qr_t *,int x,int y);
