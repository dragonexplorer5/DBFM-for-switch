/* hb_store.c - minimal implementation using cJSON and libcurl when available.
 * This implementation reads repository entries from romfs/saved_urls.json to
 * build a small in-memory catalog. If a repository URL points to a remote
 * JSON listing, it attempts to download it via libcurl (download_url_to_memory)
 * and parse it using cJSON (third_party/cJSON).
 */

#include "hb_store.h"
#include "../third_party/cJSON.h"
#include "net/downloader.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static HomebrewApp* s_app_cache = NULL;
static size_t s_cache_count = 0;

// Simple helper: parse a JSON array of apps where each item contains 'name' and 'url'
static void parse_apps_from_json(const char* json) {
    if (!json) return;
    cJSON *root = cJSON_Parse(json);
    if (!root) return;
    if (root->type != cJSON_Array) { cJSON_Delete(root); return; }

    // Count elements
    size_t add_count = 0;
    for (cJSON *it = root->child; it; it = it->next) add_count++;
    size_t old_count = s_cache_count;
    HomebrewApp* new_cache = realloc(s_app_cache, sizeof(HomebrewApp) * (old_count + add_count));
    if (!new_cache) { cJSON_Delete(root); return; }
    s_app_cache = new_cache;

    size_t idx = 0;
    for (cJSON *it = root->child; it; it = it->next) {
        cJSON *name = cJSON_GetObjectItemCaseSensitive(it, "name");
        cJSON *url = cJSON_GetObjectItemCaseSensitive(it, "url");
        HomebrewApp *app = &s_app_cache[old_count + idx];
        memset(app, 0, sizeof(*app));
    if (name && name->valuestring) snprintf(app->name, sizeof(app->name), "%s", name->valuestring);
    if (url && url->valuestring) snprintf(app->url, sizeof(app->url), "%s", url->valuestring);
    snprintf(app->title, sizeof(app->title), "%s", app->name);
        idx++;
    }

    s_cache_count = old_count + add_count;
    cJSON_Delete(root);
}

Result hbstore_init(void) {
    // Load local repo list from romfs/saved_urls.json (if present)
    FILE* f = fopen("romfs/saved_urls.json", "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long len = ftell(f);
        fseek(f, 0, SEEK_SET);
        char *buf = malloc(len + 1);
        if (buf) {
            fread(buf, 1, len, f);
            buf[len] = '\0';
            parse_apps_from_json(buf);
            free(buf);
        }
        fclose(f);
    }
    return 0;
}

void hbstore_exit(void) {
    if (s_app_cache) {
        free(s_app_cache);
        s_app_cache = NULL;
        s_cache_count = 0;
    }
}

Result hbstore_refresh_cache(void) {
    // For now, attempt to fetch each app URL and parse if it's JSON list
    for (size_t i = 0; i < s_cache_count; ++i) {
        char *buf = NULL; size_t len = 0;
        if (download_url_to_memory(s_app_cache[i].url, &buf, &len) == 0 && buf) {
            parse_apps_from_json(buf);
            free(buf);
        }
    }
    return 0;
}

Result hbstore_list_apps(HomebrewApp** out_apps, size_t* out_count) {
    if (out_apps) *out_apps = s_app_cache;
    if (out_count) *out_count = s_cache_count;
    return 0;
}

Result hbstore_get_app_info(const char* app_name, HomebrewApp* out_app) {
    if (!app_name || !out_app) return (Result)-1;
    for (size_t i = 0; i < s_cache_count; ++i) {
        if (strcmp(s_app_cache[i].name, app_name) == 0) {
            memcpy(out_app, &s_app_cache[i], sizeof(HomebrewApp));
            return 0;
        }
    }
    return (Result)-1;
}

// The following functions are placeholders for higher-level operations. They
// should be implemented with actual download/install logic when desired.
Result hbstore_download_app(const char* app_name, ProgressCallback progress_cb) {
    (void)app_name; (void)progress_cb; return (Result)-1;
}

Result hbstore_install_app(const char* app_name, ProgressCallback progress_cb) {
    (void)app_name; (void)progress_cb; return (Result)-1;
}

Result hbstore_uninstall_app(const char* app_name) { (void)app_name; return (Result)-1; }

Result hbstore_update_app(const char* app_name, ProgressCallback progress_cb) { (void)app_name; (void)progress_cb; return (Result)-1; }

Result hbstore_verify_app(const char* app_name) { (void)app_name; return (Result)-1; }

Result hbstore_check_updates(HomebrewApp** updates, size_t* count) { if (updates) *updates = NULL; if (count) *count = 0; return (Result)-1; }

Result hbstore_update_all(ProgressCallback progress_cb) { (void)progress_cb; return (Result)-1; }

bool hbstore_has_updates(void) { return false; }

Result hbstore_search(const char* query, HomebrewApp** results, size_t* count) { (void)query; if (results) *results = NULL; if (count) *count = 0; return (Result)-1; }

Result hbstore_search_by_category(const char* category, HomebrewApp** results, size_t* count) { (void)category; if (results) *results = NULL; if (count) *count = 0; return (Result)-1; }

Result hbstore_search_by_author(const char* author, HomebrewApp** results, size_t* count) { (void)author; if (results) *results = NULL; if (count) *count = 0; return (Result)-1; }

Result hbstore_clear_cache(void) { return (Result)-1; }
Result hbstore_validate_cache(void) { return (Result)-1; }

Result hbstore_load_config(void) { return (Result)-1; }
Result hbstore_save_config(void) { return (Result)-1; }
Result hbstore_get_config(StoreConfig* out_config) { (void)out_config; return (Result)-1; }
Result hbstore_set_config(const StoreConfig* config) { (void)config; return (Result)-1; }

Result hbstore_add_repository(const char* name, const char* url, const char* sig_key) { (void)name; (void)url; (void)sig_key; return (Result)-1; }
Result hbstore_remove_repository(const char* name) { (void)name; return (Result)-1; }
Result hbstore_enable_repository(const char* name, bool enable) { (void)name; (void)enable; return (Result)-1; }
Result hbstore_update_repositories(ProgressCallback progress_cb) { (void)progress_cb; return (Result)-1; }
Result hbstore_get_repositories(Repository** repos, size_t* count) { if (repos) *repos = NULL; if (count) *count = 0; return (Result)-1; }

void hbstore_render_app_list(int start_row, int selected_row, const HomebrewApp* apps, size_t count) { (void)start_row; (void)selected_row; (void)apps; (void)count; }
void hbstore_render_app_details(const HomebrewApp* app) { (void)app; }
void hbstore_render_repository_list(int start_row, int selected_row, const Repository* repos, size_t count) { (void)start_row; (void)selected_row; (void)repos; (void)count; }
void hbstore_render_update_list(int start_row, int selected_row, const HomebrewApp* updates, size_t count) { (void)start_row; (void)selected_row; (void)updates; (void)count; }
