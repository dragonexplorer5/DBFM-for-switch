#include <string.h>
#include <curl/curl.h>
#include <jansson.h>
#include "hb_store.h"

#define HB_STORE_API "https://api.github.com/repos/switchbrew/switch-homebrew-store/contents/packages"
#define DOWNLOAD_BUFFER_SIZE (1024 * 1024)

static HomebrewApp* app_cache = NULL;
static int cache_count = 0;
static u8* download_buffer = NULL;

static size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    char** response = (char**)userp;
    
    *response = realloc(*response, realsize + 1);
    if (*response) {
        memcpy(*response, contents, realsize);
        (*response)[realsize] = 0;
    }
    
    return realsize;
}

Result hbstore_init(void) {
    Result rc = 0;
    
    // Initialize download buffer
    if (!download_buffer) {
        download_buffer = (u8*)malloc(DOWNLOAD_BUFFER_SIZE);
        if (!download_buffer) return -1;
    }
    
    // Initialize curl
    curl_global_init(CURL_GLOBAL_DEFAULT);
    
    return rc;
}

Result hbstore_exit(void) {
    if (app_cache) {
        free(app_cache);
        app_cache = NULL;
        cache_count = 0;
    }
    
    if (download_buffer) {
        free(download_buffer);
        download_buffer = NULL;
    }
    
    curl_global_cleanup();
    return 0;
}

Result hbstore_refresh(void) {
    Result rc = 0;
    CURL* curl = curl_easy_init();
    if (!curl) return -1;
    
    char* response = NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, HB_STORE_API);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Switch-Homebrew-Store/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        curl_easy_cleanup(curl);
        if (response) free(response);
        return -2;
    }
    
    // Parse JSON response
    json_error_t error;
    json_t* root = json_loads(response, 0, &error);
    free(response);
    
    if (!root) {
        curl_easy_cleanup(curl);
        return -3;
    }
    
    // Clear existing cache
    if (app_cache) {
        free(app_cache);
        app_cache = NULL;
        cache_count = 0;
    }
    
    // Count array elements
    cache_count = json_array_size(root);
    if (cache_count > 0) {
        app_cache = (HomebrewApp*)calloc(cache_count, sizeof(HomebrewApp));
        if (app_cache) {
            for (int i = 0; i < cache_count; i++) {
                json_t* item = json_array_get(root, i);
                json_t* name = json_object_get(item, "name");
                json_t* description = json_object_get(item, "description");
                json_t* version = json_object_get(item, "version");
                json_t* author = json_object_get(item, "author");
                json_t* url = json_object_get(item, "download");
                
                if (json_is_string(name)) strncpy(app_cache[i].name, json_string_value(name), sizeof(app_cache[i].name) - 1);
                if (json_is_string(description)) strncpy(app_cache[i].description, json_string_value(description), sizeof(app_cache[i].description) - 1);
                if (json_is_string(version)) strncpy(app_cache[i].version, json_string_value(version), sizeof(app_cache[i].version) - 1);
                if (json_is_string(author)) strncpy(app_cache[i].author, json_string_value(author), sizeof(app_cache[i].author) - 1);
                if (json_is_string(url)) strncpy(app_cache[i].url, json_string_value(url), sizeof(app_cache[i].url) - 1);
            }
        }
    }
    
    json_decref(root);
    curl_easy_cleanup(curl);
    return rc;
}

Result hbstore_list_apps(HomebrewApp** out_apps, int* out_count) {
    if (!app_cache) {
        Result rc = hbstore_refresh();
        if (R_FAILED(rc)) return rc;
    }
    
    *out_apps = app_cache;
    *out_count = cache_count;
    return 0;
}

Result hbstore_download_app(const char* app_name) {
    Result rc = 0;
    
    // Find app in cache
    HomebrewApp* app = NULL;
    for (int i = 0; i < cache_count; i++) {
        if (strcmp(app_cache[i].name, app_name) == 0) {
            app = &app_cache[i];
            break;
        }
    }
    
    if (!app) return -1;
    
    // Download using curl
    CURL* curl = curl_easy_init();
    if (!curl) return -2;
    
    FILE* fp = fopen(app_name, "wb");
    if (!fp) {
        curl_easy_cleanup(curl);
        return -3;
    }
    
    curl_easy_setopt(curl, CURLOPT_URL, app->url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Switch-Homebrew-Store/1.0");
    
    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) rc = -4;
    
    fclose(fp);
    curl_easy_cleanup(curl);
    return rc;
}

Result hbstore_get_updates(HomebrewApp** out_updates, int* out_count) {
    // TODO: Implement update checking
    // 1. Get installed versions
    // 2. Compare with store versions
    // 3. Return apps with updates
    
    *out_updates = NULL;
    *out_count = 0;
    return 0;
}

Result hbstore_search(const char* query, HomebrewApp** out_results, int* out_count) {
    if (!app_cache) {
        Result rc = hbstore_refresh();
        if (R_FAILED(rc)) return rc;
    }
    
    // Allocate temporary array for results
    HomebrewApp* results = (HomebrewApp*)malloc(cache_count * sizeof(HomebrewApp));
    if (!results) return -1;
    
    int count = 0;
    
    // Simple case-insensitive search
    for (int i = 0; i < cache_count; i++) {
        if (strcasestr(app_cache[i].name, query) || 
            strcasestr(app_cache[i].description, query) ||
            strcasestr(app_cache[i].author, query)) {
            memcpy(&results[count++], &app_cache[i], sizeof(HomebrewApp));
        }
    }
    
    if (count > 0) {
        // Shrink array to actual size
        *out_results = realloc(results, count * sizeof(HomebrewApp));
        if (!*out_results) *out_results = results;
    } else {
        free(results);
        *out_results = NULL;
    }
    
    *out_count = count;
    return 0;
}