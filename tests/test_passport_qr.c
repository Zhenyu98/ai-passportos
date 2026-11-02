#include "passport_qr.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static void export_pgm(const char *name,const passport_qr_t *qr) {
    FILE *file=fopen(name,"wb"); assert(file);
    fprintf(file,"P5\n196 196\n255\n");
    for(int y=0;y<196;++y) for(int x=0;x<196;++x)
        fputc(passport_qr_dark_pixel(qr,x,y)?0:255,file);
    assert(fclose(file)==0);
}
static void margins(const passport_qr_t *qr) {
    assert(qr->modules>0 && qr->scale>=3);
    assert(qr->origin>=4*qr->scale);
    assert(196-qr->origin-qr->modules*qr->scale>=4*qr->scale);
    for(int y=0;y<196;++y) for(int x=0;x<196;++x)
        if(x<4*qr->scale || y<4*qr->scale || x>=196-4*qr->scale || y>=196-4*qr->scale)
            assert(!passport_qr_dark_pixel(qr,x,y));
}
int main(void) {
    passport_qr_t qr;
    assert(!passport_qr_encode("",&qr,196));
    assert(!passport_qr_encode("https://",&qr,196));
    assert(passport_qr_encode("https://example.org/a",&qr,196)); margins(&qr);
    export_pgm("build/host-tests/qr-short.pgm",&qr);
    uint8_t original[512]; memcpy(original,qr.bytes,sizeof original);
    assert(passport_qr_encode("https://example.org/b",&qr,196)); margins(&qr);
    assert(memcmp(original,qr.bytes,sizeof original)!=0);
    export_pgm("build/host-tests/qr-replaced.pgm",&qr);
    char maximum[129]; memset(maximum,'a',128); maximum[128]=0;
    memcpy(maximum,"https://example.org/",strlen("https://example.org/"));
    assert(passport_qr_encode(maximum,&qr,196)); margins(&qr);
    export_pgm("build/host-tests/qr-maximum.pgm",&qr);
    assert(!passport_qr_module(&qr,-1,0));
    assert(!passport_qr_module(&qr,0,qr.modules));
    assert(!passport_qr_dark_pixel(&qr,196,0));
    puts("Passport QR generation and quiet zone: PASS");
    return 0;
}
