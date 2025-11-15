#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <switch.h>

#include "../settings.h"
#include "../ui.h"
#include "../parental.h"
#include "../crypto.h"

static int parental_set_pin_from_text(const char *pin_text) {
    if (!pin_text || !pin_text[0]) return -1;
    unsigned char salt[16];
    crypto_random_bytes(salt, sizeof(salt));
    unsigned char out[32];
    const int iterations = 12000;
    if (pbkdf2_hmac_sha256(pin_text, salt, sizeof(salt), iterations, out, sizeof(out)) != 0) return -1;
    char salthex[sizeof(salt)*2 + 1];
    char hashhex[sizeof(out)*2 + 1];
    bin_to_hex_s(salt, sizeof(salt), salthex, sizeof(salthex));
    bin_to_hex_s(out, sizeof(out), hashhex, sizeof(hashhex));
    strncpy(g_settings.parental_pin_salt, salthex, sizeof(g_settings.parental_pin_salt)-1);
    g_settings.parental_pin_salt[sizeof(g_settings.parental_pin_salt)-1] = '\0';
    strncpy(g_settings.parental_pin_hash, hashhex, sizeof(g_settings.parental_pin_hash)-1);
    g_settings.parental_pin_hash[sizeof(g_settings.parental_pin_hash)-1] = '\0';
    save_settings();
    return 0;
}

void parental_applet_show(int view_rows, int view_cols) {
    PadState pad; padInitializeDefault(&pad);
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);

    int sel = 0;
    while (appletMainLoop()) {
        const char *lines[8];
        char tmp[512];
        int n = 0;

        snprintf(tmp, sizeof(tmp), "Parental enabled: %s", g_settings.parental_enabled ? "Yes" : "No");
        lines[n++] = strdup(tmp);
        lines[n++] = "Change PIN";
        snprintf(tmp, sizeof(tmp), "Webhook: %s", g_settings.parental_webhook[0] ? g_settings.parental_webhook : "(not set)");
        lines[n++] = strdup(tmp);
        snprintf(tmp, sizeof(tmp), "Report interval days: %d", g_settings.parental_report_days);
        lines[n++] = strdup(tmp);
        lines[n++] = "Send report now";
        lines[n++] = "Back";

        render_text_view(0, sel, lines, n, view_rows, view_cols);

        // free duplicated strings
        free((void*)lines[0]);
        free((void*)lines[2]);
        free((void*)lines[3]);

        padUpdate(&pad);
        u64 kd = padGetButtonsDown(&pad);
        if (padGetButtons(&pad) & HidNpadButton_StickRUp) { /* noop: keep compatibility */ }
        if (kd & HidNpadButton_Down) sel = (sel + 1) % n;
        if (kd & HidNpadButton_Up) sel = (sel - 1 + n) % n;
        if (kd & HidNpadButton_A) {
            if (sel == 0) {
                g_settings.parental_enabled = !g_settings.parental_enabled;
                save_settings();
            } else if (sel == 1) {
                char pinbuf[64] = {0};
                if (ui_keyboard_input(view_rows, "Enter new PIN (digits)", pinbuf, sizeof(pinbuf))) {
                    parental_set_pin_from_text(pinbuf);
                }
            } else if (sel == 2) {
                char wh[256]; strncpy(wh, g_settings.parental_webhook, sizeof(wh)-1); wh[sizeof(wh)-1]=0;
                if (ui_keyboard_input(view_rows, "Edit webhook URL", wh, sizeof(wh))) {
                    strncpy(g_settings.parental_webhook, wh, sizeof(g_settings.parental_webhook)-1);
                    g_settings.parental_webhook[sizeof(g_settings.parental_webhook)-1] = '\0';
                    save_settings();
                }
            } else if (sel == 3) {
                char daysbuf[16]; snprintf(daysbuf, sizeof(daysbuf), "%d", g_settings.parental_report_days);
                if (ui_keyboard_input(view_rows, "Report interval days", daysbuf, sizeof(daysbuf))) {
                    int v = atoi(daysbuf); if (v < 0) v = 0;
                    g_settings.parental_report_days = v;
                    save_settings();
                }
            } else if (sel == 4) {
                int res = parental_force_report();
                // small transient message (printed below UI)
                char msg[128];
                if (res == 0) snprintf(msg, sizeof(msg), "Report sent OK");
                else snprintf(msg, sizeof(msg), "Report failed (code %d)", res);
                printf("\x1b[%d;1H%s\x1b[K\n", view_rows + 2, msg); fflush(stdout);
            } else if (sel == 5) {
                // Back
                break;
            }
        }
        if (kd & HidNpadButton_B) break;
        consoleUpdate(NULL);
    }
}

// The settings structures and helpers are declared in settings.h.
// No local re-declaration is required here.