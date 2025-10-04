#include "creds.h"
#include "../third_party/cJSON.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static const char *creds_path = "sdmc:/DBFM/WEB/Passwords/Passwords.json";

int load_credentials(CredEntry **out_entries) {
    *out_entries = NULL;
    FILE *f = fopen(creds_path, "r"); if (!f) return 0;
    fseek(f, 0, SEEK_END); long L = ftell(f); fseek(f, 0, SEEK_SET);
    if (L <= 0) { fclose(f); return 0; }
    char *buf = malloc(L + 1); size_t rn = fread(buf, 1, L, f); buf[rn] = '\0'; fclose(f);
    cJSON *root = cJSON_Parse(buf); free(buf);
    if (!root) return 0;
    // expect array
    int count = 0; cJSON *it = root->child; while (it) { count++; it = it->next; }
    if (count == 0) { cJSON_Delete(root); return 0; }
    CredEntry *arr = malloc(sizeof(CredEntry) * count); memset(arr, 0, sizeof(CredEntry) * count);
    int idx = 0; it = root->child;
    while (it && idx < count) {
        cJSON *s = cJSON_GetObjectItemCaseSensitive(it, "site");
        cJSON *u = cJSON_GetObjectItemCaseSensitive(it, "username");
        cJSON *p = cJSON_GetObjectItemCaseSensitive(it, "password");
        if (s && cJSON_IsString(s) && cJSON_GetStringValue(s)) strncpy(arr[idx].site, cJSON_GetStringValue(s), sizeof(arr[idx].site)-1);
        if (u && cJSON_IsString(u) && cJSON_GetStringValue(u)) strncpy(arr[idx].username, cJSON_GetStringValue(u), sizeof(arr[idx].username)-1);
        if (p && cJSON_IsString(p) && cJSON_GetStringValue(p)) strncpy(arr[idx].password, cJSON_GetStringValue(p), sizeof(arr[idx].password)-1);
        idx++; it = it->next;
    }
    cJSON_Delete(root);
    *out_entries = arr; return count;
}

int save_credentials(CredEntry *entries, int count) {
    if (!entries || count <= 0) return -1;
    // ensure folders exist
    mkdir("sdmc:/DBFM", 0755); mkdir("sdmc:/DBFM/WEB", 0755); mkdir("sdmc:/DBFM/WEB/Passwords", 0755);
    cJSON *arr = (cJSON*)malloc(sizeof(cJSON)); memset(arr,0,sizeof(cJSON)); arr->type = cJSON_Array; cJSON *last = NULL;
    for (int i = 0; i < count; ++i) {
        cJSON *obj = (cJSON*)malloc(sizeof(cJSON)); memset(obj,0,sizeof(cJSON)); obj->type = cJSON_Object;
        // site
        cJSON *s = (cJSON*)malloc(sizeof(cJSON)); memset(s,0,sizeof(cJSON)); s->type = cJSON_String; s->string = strdup("site"); s->valuestring = strdup(entries[i].site);
        // username
        cJSON *u = (cJSON*)malloc(sizeof(cJSON)); memset(u,0,sizeof(cJSON)); u->type = cJSON_String; u->string = strdup("username"); u->valuestring = strdup(entries[i].username);
        // password
        cJSON *p = (cJSON*)malloc(sizeof(cJSON)); memset(p,0,sizeof(cJSON)); p->type = cJSON_String; p->string = strdup("password"); p->valuestring = strdup(entries[i].password);
        s->next = u; u->prev = s; u->next = p; p->prev = u; obj->child = s;
        if (!arr->child) arr->child = obj;
        if (last) {
            last->next = obj; obj->prev = last;
        }
        last = obj;
    }
    // serialize naive
    FILE *f = fopen(creds_path, "w"); if (!f) return -1;
    fprintf(f, "[\n");
    cJSON *it = arr->child; int first = 1;
    while (it) {
        if (!first) fprintf(f, ",\n");
        first = 0;
        char *n = it->child->valuestring; char *un = it->child->next->valuestring; char *pw = it->child->next->next->valuestring;
        // escape minimal
        fprintf(f, "  { \"site\": \"%s\", \"username\": \"%s\", \"password\": \"%s\" }", n ? n : "", un ? un : "", pw ? pw : "");
        it = it->next;
    }
    fprintf(f, "\n]\n"); fclose(f);
    // free arr allocated nodes (we only allocated lightweight structures above)
    it = arr->child;
    while (it) {
        cJSON *n = it->next;
        if (it->child) {
            if (it->child->string) free(it->child->string);
            if (it->child->valuestring) free(it->child->valuestring);
            if (it->child->next) {
                if (it->child->next->string) free(it->child->next->string);
                if (it->child->next->valuestring) free(it->child->next->valuestring);
                if (it->child->next->next) {
                    if (it->child->next->next->string) free(it->child->next->next->string);
                    if (it->child->next->next->valuestring) free(it->child->next->next->valuestring);
                    free(it->child->next->next);
                }
                free(it->child->next);
            }
            free(it->child);
        }
        if (it->string) free(it->string);
        free(it);
        it = n;
    }
    free(arr);
    return 0;
}

void free_creds(CredEntry *entries, int count) {
    if (entries) free(entries);
}
