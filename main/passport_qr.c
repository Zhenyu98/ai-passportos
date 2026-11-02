#include "passport_qr.h"
#include "passport_profile.h"
#include "qrcodegen.h"
#include <string.h>

_Static_assert(qrcodegen_BUFFER_LEN_FOR_VERSION(9)<=512,"QR buffer capacity");

bool passport_qr_encode(const char *url,passport_qr_t *out,int canvas) {
    if (!out) return false;
    memset(out,0,sizeof *out);
    if (!url || !*url || !passport_homepage_validate(url)) return false;
    uint8_t temporary[512];
    if (!qrcodegen_encodeText(url,temporary,out->bytes,qrcodegen_Ecc_MEDIUM,
                             1,9,qrcodegen_Mask_AUTO,false)) return false;
    out->modules=qrcodegen_getSize(out->bytes);
    if (out->modules<=0) return false;
    out->canvas=canvas;
    out->scale=canvas/(out->modules+8);
    out->origin=(canvas-out->modules*out->scale)/2;
    if (out->scale<3 || out->origin<4*out->scale) {
        memset(out,0,sizeof *out);
        return false;
    }
    return true;
}
bool passport_qr_module(const passport_qr_t *qr,int x,int y) {
    return qr && qr->modules>0 && x>=0 && y>=0 && x<qr->modules && y<qr->modules
        && qrcodegen_getModule(qr->bytes,x,y);
}
bool passport_qr_dark_pixel(const passport_qr_t *qr,int x,int y) {
    if (!qr || qr->scale<3 || x<qr->origin || y<qr->origin || x>=qr->canvas || y>=qr->canvas) return false;
    return passport_qr_module(qr,(x-qr->origin)/qr->scale,(y-qr->origin)/qr->scale);
}
