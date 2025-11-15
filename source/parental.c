#include "parental.h"
#include "settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "crypto.h"
#include <sys/stat.h>

#ifdef USE_LIBCURL
#include <curl/curl.h>
#endif

// Simple log file under DBFM
static const char *logpath = "sdmc:/DBFM/hello-world/parental_log.txt";

int parental_is_enabled(void) {
    return g_settings.parental_enabled;
}

int parental_check_pin(const char *pin) {
    if (!g_settings.parental_enabled) return 1;
    if (!pin) return 0;
    // If no stored hash, deny
    if (!g_settings.parental_pin_hash[0] || !g_settings.parental_pin_salt[0]) return 0;
    unsigned char salt[32]; unsigned char out[32];
    int salt_len = hex_to_bin(g_settings.parental_pin_salt, salt, sizeof(salt));
    if (salt_len <= 0) return 0;
    // derive using actual salt length
    if (pbkdf2_hmac_sha256(pin, salt, (size_t)salt_len, 12000, out, sizeof(out)) != 0) return 0;
    char hex[65]; bin_to_hex_s(out, 32, hex, sizeof(hex));
    return strcmp(hex, g_settings.parental_pin_hash) == 0;
}

void parental_log_action(const char *action, const char *details) {
    FILE *f = fopen(logpath, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tm; localtime_r(&t, &tm);
    char tbuf[64]; strftime(tbuf, sizeof(tbuf), "%Y-%m-%d %H:%M:%S", &tm);
    fprintf(f, "%s | %s | %s\n", tbuf, action ? action : "(action)", details ? details : "");
    fclose(f);
    // Rotate if file exceeds ~64KB
    struct stat st; if (stat(logpath, &st) == 0) {
        if (st.st_size > 64*1024) {
            // keep last 32KB
            FILE *rf = fopen(logpath, "r"); if (rf) {
                if (fseek(rf, -32*1024, SEEK_END) == 0) {
                    char *buf = malloc(32*1024);
                    size_t rn = fread(buf, 1, 32*1024, rf);
                    fclose(rf);
                    FILE *wf = fopen(logpath, "w"); if (wf) { fwrite(buf, 1, rn, wf); fclose(wf); }
                    free(buf);
                } else fclose(rf);
            }
        }
    }
}

void parental_maybe_report(void) {
    if (!g_settings.parental_enabled) return;
    // If webhook not configured, skip
    if (!g_settings.parental_webhook[0]) return;

    // Check interval: if parental_report_days is zero, no auto-reporting
    if (g_settings.parental_report_days <= 0) return;
    time_t now = time(NULL);
    long last = (long)g_settings.parental_last_report;
    long needed = (long)g_settings.parental_report_days * 24L * 3600L;
    if (last != 0 && (now - last) < needed) return; // not yet time

    // perform report
    int ok = 0;
#ifdef USE_LIBCURL
    FILE *f = fopen(logpath, "r");
    if (f) {
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        long seek = sz - 4096; if (seek < 0) seek = 0;
        fseek(f, seek, SEEK_SET);
        char *buf = malloc(4096 + 1);
        size_t rn = fread(buf, 1, 4096, f); buf[rn] = '\0'; fclose(f);

        CURL *curl = curl_easy_init();
        if (curl) {
            curl_easy_setopt(curl, CURLOPT_URL, g_settings.parental_webhook);
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);
            CURLcode cres = curl_easy_perform(curl);
            if (cres == CURLE_OK) ok = 1;
            curl_easy_cleanup(curl);
        }
        free(buf);
    }
#else
    (void)logpath; (void)now; (void)last; (void)needed;
#endif

    if (ok) {
        settings_mark_parental_report((long)time(NULL));
    }
}

// Force an immediate report regardless of last_report timestamp. Returns 0 on success, -1 on failure.
int parental_force_report(void) {
    if (!g_settings.parental_enabled) return -1;
    if (!g_settings.parental_webhook[0]) return -1;
    int ok = 0;
#ifdef USE_LIBCURL
    FILE *f = fopen(logpath, "r");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    long seek = sz - 8192; if (seek < 0) seek = 0;
    fseek(f, seek, SEEK_SET);
    char *buf = malloc(8192 + 1);
    size_t rn = fread(buf, 1, 8192, f); buf[rn] = '\0'; fclose(f);
    CURL *curl = curl_easy_init();
    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, g_settings.parental_webhook);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, buf);
        if (curl_easy_perform(curl) == CURLE_OK) ok = 1;
        curl_easy_cleanup(curl);
    }
    free(buf);
#endif
    if (ok) { settings_mark_parental_report((long)time(NULL)); return 0; }
    return -1;
}
