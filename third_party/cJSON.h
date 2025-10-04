/*
  cJSON.h - MIT License
  This is the single-file public header for cJSON (truncated minimal needed parts)
  For full project see https://github.com/DaveGamble/cJSON
*/

#ifndef CJSON_H
#define CJSON_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct cJSON {
    struct cJSON *next;
    struct cJSON *prev;
    struct cJSON *child;

    int type;

    char *valuestring;
    int valueint;
    double valuedouble;

    char *string;
} cJSON;

/* cJSON Types: */
#define cJSON_False 0
#define cJSON_True 1
#define cJSON_NULL 2
#define cJSON_Number 3
#define cJSON_String 4
#define cJSON_Array 5
#define cJSON_Object 6

/* Parse JSON text to a cJSON object */
extern cJSON *cJSON_Parse(const char *value);
/* Delete a cJSON object */
extern void cJSON_Delete(cJSON *c);
/* Get object item by key (case sensitive) */
extern cJSON *cJSON_GetObjectItemCaseSensitive(const cJSON *object, const char *string);
/* Returns 1 if the item is a string */
extern int cJSON_IsString(const cJSON *item);
/* Get string value */
extern const char *cJSON_GetStringValue(const cJSON *item);

#ifdef __cplusplus
}
#endif

#endif /* CJSON_H */
