#include "passport_profile.h"
#include <string.h>

bool passport_utf8_next(const char *text, size_t length, size_t *offset, uint32_t *out) {
    if (!text || !offset || !out || *offset >= length) return false;
    size_t at = *offset;
    uint8_t first = (uint8_t)text[at++];
    uint32_t value, minimum;
    unsigned extra;
    if (first < 0x80) { value = first; minimum = 0; extra = 0; }
    else if (first >= 0xc2 && first <= 0xdf) { value = first & 0x1f; minimum = 0x80; extra = 1; }
    else if (first >= 0xe0 && first <= 0xef) { value = first & 0x0f; minimum = 0x800; extra = 2; }
    else if (first >= 0xf0 && first <= 0xf4) { value = first & 7; minimum = 0x10000; extra = 3; }
    else return false;
    if (extra > length - at) return false;
    for (unsigned i = 0; i < extra; ++i) {
        uint8_t next = (uint8_t)text[at++];
        if ((next & 0xc0) != 0x80) return false;
        value = (value << 6) | (next & 0x3f);
    }
    if (value < minimum || value > 0x10ffff || (value >= 0xd800 && value <= 0xdfff)) return false;
    *offset = at; *out = value;
    return true;
}

static bool text_valid(const char *text, size_t maximum, bool multiline) {
    const char *end = memchr(text, 0, maximum + 1);
    if (!end) return false;
    size_t length = (size_t)(end - text), offset = 0;
    while (offset < length) {
        uint32_t cp;
        if (!passport_utf8_next(text, length, &offset, &cp)) return false;
        if ((cp < 0x20 && !(multiline && cp == '\n')) || (cp >= 0x7f && cp <= 0x9f)) return false;
    }
    return true;
}

static bool hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool prefix_equal(const char *s, const char *prefix) {
    for (; *prefix; ++prefix, ++s) {
        unsigned char c = (unsigned char)*s;
        if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
        if (c != (unsigned char)*prefix) return false;
    }
    return true;
}

static bool ipv4_valid(const char *p, const char *end) {
    for (unsigned part = 0; part < 4; ++part) {
        unsigned value = 0, digits = 0;
        while (p < end && *p >= '0' && *p <= '9') {
            value = value * 10 + (unsigned)(*p++ - '0'); ++digits;
            if (digits > 3 || value > 255) return false;
        }
        if (!digits) return false;
        if (part == 3) return p == end;
        if (p == end || *p++ != '.') return false;
    }
    return false;
}

static bool ipv6_valid(const char *p, const char *end) {
    unsigned units = 0;
    bool compressed = false;
    if (p == end) return false;
    if (*p == ':') {
        if (end - p < 2 || p[1] != ':') return false;
        compressed = true; p += 2;
    }
    while (p < end) {
        const char *start = p;
        unsigned digits = 0;
        while (p < end && hex_digit(*p)) { ++p; ++digits; }
        if (p < end && *p == '.') {
            if (!ipv4_valid(start, end)) return false;
            units += 2; p = end; break;
        }
        if (!digits || digits > 4 || ++units > 8) return false;
        if (p == end) break;
        if (*p++ != ':' || p == end) return false;
        if (*p == ':') {
            if (compressed) return false;
            compressed = true; ++p;
        }
    }
    return compressed ? units < 8 : units == 8;
}

bool passport_homepage_validate(const char *url) {
    if (!url || !text_valid(url, PASSPORT_HOMEPAGE_MAX, false)) return false;
    if (!*url) return true;
    const char *host;
    if (prefix_equal(url, "https://")) host = url + 8;
    else if (prefix_equal(url, "http://")) host = url + 7;
    else return false;
    for (const char *p = url; *p; ++p) {
        if ((unsigned char)*p <= 0x20 || *p == '\\') return false;
        if (*p == '%' && (!p[1] || !p[2] || !hex_digit(p[1]) || !hex_digit(p[2]))) return false;
    }
    const char *end = host;
    while (*end && *end != '/' && *end != '?' && *end != '#') ++end;
    for (const char *p = host; p < end; ++p) if (*p == '@') host = p + 1;
    if (host == end) return false;
    const char *port = NULL;
    if (*host == '[') {
        const char *close = memchr(host, ']', (size_t)(end - host));
        if (!close || !ipv6_valid(host + 1, close)) return false;
        if (close + 1 != end) {
            if (close[1] != ':') return false;
            port = close + 2;
        }
    } else {
        const char *host_end = host;
        while (host_end < end && *host_end != ':') ++host_end;
        if (host_end == host) return false;
        bool meaningful = false;
        for (const char *p = host; p < host_end; ++p) {
            unsigned char c = (unsigned char)*p;
            if (c >= 0x80 || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) meaningful = true;
            else if (c != '.' && c != '-' && c != '_' && c != '%' && c != '~') return false;
        }
        if (!meaningful) return false;
        if (host_end < end) port = host_end + 1;
    }
    if (port) {
        unsigned value = 0;
        if (port == end) return false;
        for (const char *p = port; p < end; ++p) {
            if (*p < '0' || *p > '9') return false;
            value = value * 10 + (unsigned)(*p - '0');
            if (value > 65535) return false;
        }
        if (!value) return false;
    }
    return true;
}

bool passport_profile_validate(const passport_profile_t *p) {
    return p && text_valid(p->name, PASSPORT_NAME_MAX, false)
        && text_valid(p->title, PASSPORT_TITLE_MAX, false)
        && text_valid(p->research, PASSPORT_RESEARCH_MAX, true)
        && text_valid(p->bio, PASSPORT_BIO_MAX, true)
        && passport_homepage_validate(p->homepage);
}

passport_status_t passport_profile_check(const passport_profile_t *p,passport_glyph_fn has,
                                         void *user,passport_glyph_issue_t *issue) {
    if(issue) *issue=(passport_glyph_issue_t){0};
    if(!p || !has || !passport_profile_validate(p)) return PASSPORT_ERR_INVALID;
    const char *fields[]={p->name,p->title,p->research,p->bio,p->homepage};
    for(unsigned field=0;field<5;field++) {
        size_t offset=0,length=strlen(fields[field]);
        while(offset<length) {
            uint32_t cp;
            if(!passport_utf8_next(fields[field],length,&offset,&cp)) return PASSPORT_ERR_INVALID;
            if(cp!='\n' && !has(cp,user)) {
                if(issue) *issue=(passport_glyph_issue_t){field,cp};
                return PASSPORT_ERR_UNSUPPORTED;
            }
        }
    }
    return PASSPORT_OK;
}
