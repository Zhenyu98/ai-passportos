#include "passport_profile.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    passport_profile_t p = {0};
    assert(passport_profile_validate(&p));
    strcpy(p.name, "Researcher");
    strcpy(p.title, "PhD researcher");
    strcpy(p.bio, "One line\nAnother line");
    strcpy(p.research, "AI\nMaterials");
    strcpy(p.homepage, "https://example.org/research?x=1#bio");
    assert(passport_profile_validate(&p));
    memset(p.name, 'a', PASSPORT_NAME_MAX);
    p.name[PASSPORT_NAME_MAX] = 0;
    assert(passport_profile_validate(&p));
    memset(p.name, 'a', sizeof p.name);
    assert(!passport_profile_validate(&p));
    memset(p.name, 0, sizeof p.name);
    memset(p.name, 'a', 61);
    memcpy(p.name + 61, "\xe4\xb8\xad", 3);
    assert(passport_profile_validate(&p));
    p.name[63] = 0;
    assert(!passport_profile_validate(&p));
    strcpy(p.name, "\xc0\xaf");
    assert(!passport_profile_validate(&p));
    strcpy(p.name, "\xed\xa0\x80");
    assert(!passport_profile_validate(&p));
    strcpy(p.name, "\xf4\x90\x80\x80");
    assert(!passport_profile_validate(&p));
    strcpy(p.name, "Name\nBad");
    assert(!passport_profile_validate(&p));
    strcpy(p.name, "Name");
    const char *bad[] = {"https://", "http:///path", "javascript:alert(1)",
                         "ftp://example.org", "https://bad host/", "https://example.org:bad/"};
    for (size_t i = 0; i < sizeof bad / sizeof bad[0]; ++i) {
        strcpy(p.homepage, bad[i]);
        assert(!passport_profile_validate(&p));
    }
    const char *good[] = {"", "http://localhost:8080/", "https://example.org", "HTTPS://example.org/a", "https://[::1]/"};
    for (size_t i = 0; i < sizeof good / sizeof good[0]; ++i) {
        strcpy(p.homepage, good[i]);
        assert(passport_profile_validate(&p));
    }
    memset(p.homepage, 'a', PASSPORT_HOMEPAGE_MAX);
    memcpy(p.homepage, "https://example.org/", strlen("https://example.org/"));
    p.homepage[PASSPORT_HOMEPAGE_MAX] = 0;
    assert(passport_profile_validate(&p));
    memset(p.homepage, 'a', sizeof p.homepage);
    assert(!passport_profile_validate(&p));
    p.homepage[0] = 0;
    memset(p.title, 't', PASSPORT_TITLE_MAX); p.title[PASSPORT_TITLE_MAX] = 0;
    memset(p.research, 'r', PASSPORT_RESEARCH_MAX); p.research[PASSPORT_RESEARCH_MAX] = 0;
    memset(p.bio, 'b', PASSPORT_BIO_MAX); p.bio[PASSPORT_BIO_MAX] = 0;
    assert(passport_profile_validate(&p));
    p.bio[0] = '\x01';
    assert(!passport_profile_validate(&p));
    memset(&p,0,sizeof p);
    char *fields[] = {p.name,p.title,p.research,p.bio,p.homepage};
    const size_t limits[] = {PASSPORT_NAME_MAX,PASSPORT_TITLE_MAX,PASSPORT_RESEARCH_MAX,PASSPORT_BIO_MAX,PASSPORT_HOMEPAGE_MAX};
    for (unsigned i=0;i<5;++i) {
        memset(fields[i],'x',limits[i]+1);
        assert(!passport_profile_validate(&p));
        memset(fields[i],0,limits[i]+1);
    }
    /* Muse scan replies drop SSIDs this rejects, e.g. a GBK-encoded name. */
    const char gbk[] = "\xce\xde\xcf\xdf\xb4\xab\xc6\xc1", utf8[] = "\xe6\x97\xa0\xe7\xba\xbf";
    size_t offset = 0; uint32_t cp;
    assert(!passport_utf8_next(gbk, sizeof gbk - 1, &offset, &cp) && offset == 0);
    while (offset < sizeof utf8 - 1) assert(passport_utf8_next(utf8, sizeof utf8 - 1, &offset, &cp));
    assert(cp == 0x7ebf);
    puts("Passport profile: PASS");
    return 0;
}
