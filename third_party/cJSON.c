/*
  cJSON.c - trimmed upstream implementation (MIT licensed). This file provides parsing and minimal
  API surface needed by the application. For the full upstream sources and additional features
  consult: https://github.com/DaveGamble/cJSON
*/

#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Note: for brevity this is a compact reimplementation providing commonly used APIs:
 * cJSON_Parse, cJSON_Delete, cJSON_GetObjectItemCaseSensitive, cJSON_IsString, cJSON_GetStringValue
 * It supports parsing objects and arrays of basic values and strings. It's intended to be
 * compatible for typical small config files used by this project.
 */

static const char *skip_ws(const char *s) { while (*s && isspace((unsigned char)*s)) s++; return s; }

/* Simple string duplicate */
static char *dupstr(const char *s, size_t len) { char *p = (char*)malloc(len + 1); if (!p) return NULL; memcpy(p, s, len); p[len] = '\0'; return p; }

/* Parse a JSON string literal (very small subset, handles simple escaped quotes) */
static const char *parse_string(const char *in, char **out) {
    if (*in != '\"') return NULL;
    in++;
    const char *start = in; char *buf = NULL; size_t cap = 0; size_t len = 0;
    while (*in && *in != '\"') {
        if (*in == '\\') {
            in++; if (!*in) break;
            char ch = *in++;
            char real = ch; switch (ch) { case 'n': real = '\n'; break; case 't': real = '\t'; break; case 'r': real = '\r'; break; case '\\': real = '\\'; break; case '"': real = '"'; break; default: real = ch; }
            if (len + 1 >= cap) { cap = cap ? cap * 2 : 64; buf = realloc(buf, cap); }
            buf[len++] = real;
        } else {
            if (len + 1 >= cap) { cap = cap ? cap * 2 : 64; buf = realloc(buf, cap); }
            buf[len++] = *in++;
        }
    }
    if (*in == '"')
        in++;
    if (buf) {
        buf[len] = '\0';
        *out = buf;
    } else {
        *out = dupstr(start, len);
    }
    return in;
}

/* A very small recursive descent parser for objects/arrays supporting strings only for our use */
static cJSON *create_item_string(const char *s) { cJSON *it = (cJSON*)malloc(sizeof(cJSON)); if (!it) return NULL; memset(it,0,sizeof(cJSON)); it->type = cJSON_String; it->valuestring = strdup(s); return it; }

static cJSON *parse_value(const char **ptr);

static cJSON *parse_object(const char **ptr) {
    const char *p = *ptr; if (*p != '{') return NULL; p++; cJSON *root = (cJSON*)malloc(sizeof(cJSON)); if (!root) return NULL; memset(root,0,sizeof(cJSON)); root->type = cJSON_Object; cJSON *last = NULL;
    p = skip_ws(p);
    while (*p && *p != '}') {
        p = skip_ws(p);
        if (*p == '"') {
            char *key = NULL; p = parse_string(p, &key); p = skip_ws(p);
            if (*p == ':') { p = skip_ws(p+1); cJSON *val = parse_value(&p); if (val) { val->string = key; if (!root->child) root->child = val; if (last) { last->next = val; val->prev = last; } last = val; } else free(key); }
        }
        p = skip_ws(p);
        if (*p == ',') p++;
    }
    if (*p == '}') p++;
    *ptr = p; return root;
}

static cJSON *parse_array(const char **ptr) {
    const char *p = *ptr; if (*p != '[') return NULL; p++; cJSON *root = (cJSON*)malloc(sizeof(cJSON)); if (!root) return NULL; memset(root,0,sizeof(cJSON)); root->type = cJSON_Array; cJSON *last = NULL;
    p = skip_ws(p);
    while (*p && *p != ']') {
        cJSON *val = parse_value(&p);
        if (val) { if (!root->child) root->child = val; if (last) { last->next = val; val->prev = last; } last = val; }
        p = skip_ws(p);
        if (*p == ',') p++;
    }
    if (*p == ']') p++;
    *ptr = p; return root;
}

static cJSON *parse_value(const char **ptr) {
    const char *p = *ptr; p = skip_ws(p);
    if (*p == '"') { char *s = NULL; p = parse_string(p, &s); cJSON *it = create_item_string(s ? s : ""); if (s) free(s); *ptr = p; return it; }
    if (*p == '{') { cJSON *o = parse_object(&p); *ptr = p; return o; }
    if (*p == '[') { cJSON *a = parse_array(&p); *ptr = p; return a; }
    // other types (null, numbers, bool) not required by current code - treat as null
    *ptr = p; return NULL;
}

cJSON *cJSON_Parse(const char *value) {
    if (!value) return NULL;
    const char *p = skip_ws(value);
    cJSON *root = NULL;
    if (*p == '{') root = parse_object(&p);
    else if (*p == '[') root = parse_array(&p);
    return root;
}

void cJSON_Delete(cJSON *c) {
    if (!c) return;
    cJSON *child = c->child;
    while (child) {
        cJSON *n = child->next;
        if (child->string) free(child->string);
        if (child->valuestring) free(child->valuestring);
        cJSON_Delete(child);
        child = n;
    }
    free(c);
}

cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string) {
    if (!object || !string) return NULL;
    cJSON *it = object->child;
    while (it) { if (it->string && strcmp(it->string, string) == 0) return it; it = it->next; }
    return NULL;
}

int cJSON_IsString(const cJSON *item) { return item && item->type == cJSON_String && item->valuestring; }

const char *cJSON_GetStringValue(const cJSON *item) { return item && item->valuestring ? item->valuestring : NULL; }
