/*
 * switch_controls.c - Implementation of Joy-Con control mapping for DBFM
 */

#include "../include/switch_controls.h"
#include <string.h>

// Frame counter for repeat handling (auto-repeat every N frames)
#define REPEAT_DELAY_FRAMES 10
#define REPEAT_INTERVAL_FRAMES 4
#define HOLD_TIME_MS_MULTI_SELECT 500

void switch_controls_init(SwitchControlState* state) {
    if (!state) return;
    memset(state, 0, sizeof(SwitchControlState));
    state->current_event = CONTROL_NONE;
    state->last_event = CONTROL_NONE;
    state->allow_repeat = true;
    state->repeat_rate = 0;
}

ControlEvent switch_controls_update(PadState* pad, SwitchControlState* state) {
    if (!pad || !state) return CONTROL_NONE;

    state->last_event = state->current_event;
    state->current_event = CONTROL_NONE;

    // Get button state
    u64 buttons_down = padGetButtonsDown(pad);
    u64 buttons_held = padGetButtons(pad);

    // Get stick input
    HidAnalogStickState left_stick = padGetStickPos(pad, 0);   // Left stick
    HidAnalogStickState right_stick = padGetStickPos(pad, 1);  // Right stick

    // Store right stick for smooth scrolling
    state->right_stick_x = right_stick.x;
    state->right_stick_y = right_stick.y;

    // ===== PRIMARY FILE OPERATIONS (ABXY) =====

    // A Button - Open or multi-select
    if (buttons_down & HidNpadButton_A) {
        state->button_hold_time = 0;
        // Check if we should enter multi-select mode (typically A is just open)
        // Multi-select triggered by holding A; will be checked in repeat logic
        state->current_event = CONTROL_OPEN;
        return state->current_event;
    }

    // Track hold time for multi-select detection
    if (buttons_held & HidNpadButton_A) {
        state->button_hold_time += 16; // Approximate frame time in ms (60 FPS ≈ 16.7 ms)
        if (state->button_hold_time > HOLD_TIME_MS_MULTI_SELECT && !state->multi_select_active) {
            state->multi_select_active = true;
            state->current_event = CONTROL_MULTI_SELECT_TOGGLE;
            return state->current_event;
        }
    } else {
        state->button_hold_time = 0;
        if (state->multi_select_active) {
            // A was released; could exit multi-select or remain (design choice)
            // For now, remain in multi-select until explicitly exited
        }
    }

    // B Button - Go back
    if (buttons_down & HidNpadButton_B) {
        state->multi_select_active = false; // Exit multi-select on back
        state->current_event = CONTROL_BACK;
        return state->current_event;
    }

    // Y Button - Context menu
    if (buttons_down & HidNpadButton_Y) {
        state->context_menu_active = !state->context_menu_active;
        state->current_event = CONTROL_CONTEXT_MENU;
        return state->current_event;
    }

    // X Button - Search
    if (buttons_down & HidNpadButton_X) {
        state->search_active = !state->search_active;
        state->current_event = CONTROL_SEARCH;
        return state->current_event;
    }

    // ===== NAVIGATION (D-Pad / Left Stick) =====

    // D-Pad Up or Left Stick Up
    if ((buttons_down & HidNpadButton_Up) || (left_stick.y > 5000)) {
        state->current_event = CONTROL_NAV_UP;
        state->repeat_rate = 0; // Reset repeat counter
        return state->current_event;
    }

    // D-Pad Down or Left Stick Down
    if ((buttons_down & HidNpadButton_Down) || (left_stick.y < -5000)) {
        state->current_event = CONTROL_NAV_DOWN;
        state->repeat_rate = 0;
        return state->current_event;
    }

    // D-Pad Left (quick parent)
    if (buttons_down & HidNpadButton_Left) {
        state->current_event = CONTROL_NAV_PARENT;
        return state->current_event;
    }

    // D-Pad Right (quick child)
    if (buttons_down & HidNpadButton_Right) {
        state->current_event = CONTROL_NAV_CHILD;
        return state->current_event;
    }

    // Handle held D-Pad for repeat navigation
    if (state->allow_repeat) {
        if ((buttons_held & HidNpadButton_Up) || (left_stick.y > 5000)) {
            state->repeat_rate++;
            if (state->repeat_rate > REPEAT_DELAY_FRAMES) {
                if ((state->repeat_rate - REPEAT_DELAY_FRAMES) % REPEAT_INTERVAL_FRAMES == 0) {
                    state->current_event = CONTROL_NAV_UP;
                    return state->current_event;
                }
            }
        } else if ((buttons_held & HidNpadButton_Down) || (left_stick.y < -5000)) {
            state->repeat_rate++;
            if (state->repeat_rate > REPEAT_DELAY_FRAMES) {
                if ((state->repeat_rate - REPEAT_DELAY_FRAMES) % REPEAT_INTERVAL_FRAMES == 0) {
                    state->current_event = CONTROL_NAV_DOWN;
                    return state->current_event;
                }
            }
        } else {
            state->repeat_rate = 0;
        }
    }

    // ===== PAGING (ZL/ZR) =====

    // ZL - Page up
    if (buttons_down & HidNpadButton_ZL) {
        state->current_event = CONTROL_PAGE_UP;
        return state->current_event;
    }

    // ZR - Page down
    if (buttons_down & HidNpadButton_ZR) {
        state->current_event = CONTROL_PAGE_DOWN;
        return state->current_event;
    }

    // ===== STORAGE TABS (L/R) =====

    // L - Previous tab
    if (buttons_down & HidNpadButton_L) {
        state->current_event = CONTROL_TAB_PREV;
        return state->current_event;
    }

    // R - Next tab
    if (buttons_down & HidNpadButton_R) {
        state->current_event = CONTROL_TAB_NEXT;
        return state->current_event;
    }

    // ===== MENUS (±) =====

    // Plus - Main menu
    if (buttons_down & HidNpadButton_Plus) {
        state->current_event = CONTROL_MAIN_MENU;
        return state->current_event;
    }

    // Minus - Settings menu
    if (buttons_down & HidNpadButton_Minus) {
        state->current_event = CONTROL_SETTINGS_MENU;
        return state->current_event;
    }

    // ===== SMOOTH SCROLL (Right Stick) =====
    if (right_stick.y != 0) {
        state->current_event = CONTROL_SCROLL_SMOOTH;
        // Note: caller will use switch_controls_get_scroll_amount() for the actual value
    }

    return state->current_event;
}

bool switch_controls_is_multi_select(const SwitchControlState* state) {
    return state ? state->multi_select_active : false;
}

int16_t switch_controls_get_scroll_amount(const SwitchControlState* state) {
    if (!state) return 0;
    // Return right stick Y; positive = down, negative = up
    // Threshold to avoid stick noise
    if (state->right_stick_y > 5000) return 1;      // Scroll down
    if (state->right_stick_y < -5000) return -1;    // Scroll up
    return 0;
}

int switch_controls_get_page_direction(ControlEvent event) {
    if (event == CONTROL_PAGE_DOWN) return 1;
    if (event == CONTROL_PAGE_UP) return -1;
    return 0;
}

bool switch_controls_should_repeat_nav(SwitchControlState* state) {
    if (!state) return false;
    // Only repeat if in a navigation event and enough time has passed
    if (state->current_event == CONTROL_NAV_UP || state->current_event == CONTROL_NAV_DOWN) {
        return (state->repeat_rate > REPEAT_DELAY_FRAMES);
    }
    return false;
}

const char* switch_controls_event_name(ControlEvent event) {
    switch (event) {
        case CONTROL_NONE:                 return "NONE";
        case CONTROL_OPEN:                 return "OPEN (A)";
        case CONTROL_BACK:                 return "BACK (B)";
        case CONTROL_CONTEXT_MENU:         return "CONTEXT_MENU (Y)";
        case CONTROL_SEARCH:               return "SEARCH (X)";
        case CONTROL_NAV_UP:               return "NAV_UP";
        case CONTROL_NAV_DOWN:             return "NAV_DOWN";
        case CONTROL_NAV_PARENT:           return "NAV_PARENT (←)";
        case CONTROL_NAV_CHILD:            return "NAV_CHILD (→)";
        case CONTROL_PAGE_UP:              return "PAGE_UP (ZL)";
        case CONTROL_PAGE_DOWN:            return "PAGE_DOWN (ZR)";
        case CONTROL_TAB_PREV:             return "TAB_PREV (L)";
        case CONTROL_TAB_NEXT:             return "TAB_NEXT (R)";
        case CONTROL_MAIN_MENU:            return "MAIN_MENU (+)";
        case CONTROL_SETTINGS_MENU:        return "SETTINGS_MENU (-)";
        case CONTROL_MULTI_SELECT_TOGGLE:  return "MULTI_SELECT (Hold A)";
        case CONTROL_SCROLL_SMOOTH:        return "SCROLL (Right Stick)";
        default:                           return "UNKNOWN";
    }
}
