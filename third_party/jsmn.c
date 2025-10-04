/* Minimal jsmn implementation. This is a small, permissive tokenizer sufficient for extracting strings.
 * Not a full-featured robust library, but adequate for small config parsing.
 */
#include "jsmn.h"
#include <string.h>

static jsmntok_t *jsmn_alloc_token(jsmn_parser *parser, jsmntok_t *tokens, unsigned int num_tokens) {
    if (parser->toknext >= (int)num_tokens) return NULL;
    jsmntok_t *tok = &tokens[parser->toknext++];
    tok->start = tok->end = -1; tok->size = 0; tok->type = JSMN_UNDEFINED;
    return tok;
}

void jsmn_init(jsmn_parser *parser) {
    parser->pos = 0; parser->toknext = 0; parser->toksuper = -1;
}

int jsmn_parse(jsmn_parser *parser, const char *js, size_t len, jsmntok_t *tokens, unsigned int num_tokens) {
    int i = 0; jsmntok_t *tok;
    for (i = 0; i < (int)len; i++) {
        char c = js[i];
        switch (c) {
            case '{': case '[':
                tok = jsmn_alloc_token(parser, tokens, num_tokens);
                if (!tok) return -1;
                tok->type = (c == '{') ? JSMN_OBJECT : JSMN_ARRAY;
                tok->start = i; tok->size = 0;
                if (parser->toksuper != -1) tokens[parser->toksuper].size++;
                parser->toksuper = parser->toknext - 1;
                break;
            case '}': case ']': {
                jsmntype_t type = (c == '}') ? JSMN_OBJECT : JSMN_ARRAY;
                int found = 0;
                int j;
                for (j = parser->toknext - 1; j >= 0; j--) {
                    if (tokens[j].start != -1 && tokens[j].end == -1) {
                        if (tokens[j].type == type) {
                            tokens[j].end = i + 1; parser->toksuper = -1; found = 1; break;
                        } else return -1;
                    }
                }
                if (!found) return -1;
                break;
            }
            case '"': {
                int start = i + 1; int j = start; while (j < (int)len) {
                        if (js[j] == '"')
                            break;
                        if (js[j] == '\\')
                            j++;
                        j++;
                }
                if (j >= (int)len) return -1;
                tok = jsmn_alloc_token(parser, tokens, num_tokens);
                if (!tok) return -1;
                tok->type = JSMN_STRING; tok->start = start; tok->end = j;
                if (parser->toksuper != -1) tokens[parser->toksuper].size++;
                i = j; break;
            }
            default:
                break;
        }
    }
    return parser->toknext;
}