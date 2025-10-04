#include "json.h"
#include "json.h"
#include "../third_party/jsmn.h"
#include <string.h>

int json_get_string_value(const char *buf, const char *key, char *out, size_t out_len) {
    if (!buf || !key || !out || out_len == 0) return 0;
    jsmn_parser parser; jsmn_init(&parser);
    jsmntok_t tokens[64];
    int ntok = jsmn_parse(&parser, buf, strlen(buf), tokens, sizeof(tokens)/sizeof(tokens[0]));
    if (ntok < 1) return 0;
    size_t keylen = strlen(key);
    for (int i = 0; i < ntok; ++i) {
        if (tokens[i].type == JSMN_STRING) {
            int kstart = tokens[i].start; int kend = tokens[i].end;
            int klen = kend - kstart;
            if ((int)keylen == klen && strncmp(buf + kstart, key, klen) == 0) {
                // value should be the next token
                if (i + 1 < ntok && tokens[i+1].type == JSMN_STRING) {
                    int vstart = tokens[i+1].start; int vend = tokens[i+1].end;
                    int vlen = vend - vstart; if (vlen >= (int)out_len) vlen = (int)out_len - 1;
                    memcpy(out, buf + vstart, vlen); out[vlen] = '\0';
                    return 1;
                }
            }
        }
    }
    return 0;
}