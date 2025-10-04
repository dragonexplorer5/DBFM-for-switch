/* cjson_compat implementation disabled to avoid duplicate symbols with bundled cJSON.c
   The project uses third_party/cJSON.c as the canonical implementation. */

#if 0
#include "cjson_compat.h"
#include <stdlib.h>
#include <string.h>
#include "../source/json.h"

struct cJSON {
    char *valuestring;
};

cJSON *cJSON_Parse(const char *json) {
    if (!json) return NULL;
    cJSON *root = (cJSON*)malloc(sizeof(cJSON));
    if (!root) return NULL;
    root->valuestring = strdup(json);
    return root;
}

cJSON *cJSON_GetObjectItemCaseSensitive(cJSON *root, const char *key) {
    if (!root || !key) return NULL;
    char val[256]; if (json_get_string_value(root->valuestring, key, val, sizeof(val))) {
        cJSON *it = (cJSON*)malloc(sizeof(cJSON)); if (!it) return NULL; it->valuestring = strdup(val); return it;
    }
    return NULL;
}

int cJSON_IsString(const cJSON *item) { return item && item->valuestring; }
const char *cJSON_GetStringValue(const cJSON *item) { return item ? item->valuestring : NULL; }

void cJSON_Delete(cJSON *root) {
    if (!root) return;
    if (root->valuestring) free(root->valuestring);
    free(root);
}
#endif