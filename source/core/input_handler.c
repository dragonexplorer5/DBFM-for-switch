#include <switch.h>
#include <math.h>
#include <stdlib.h>
#include "input_handler.h"

#define SHAKE_THRESHOLD 2.0f
#define SHAKE_COOLDOWN 500000000ULL  // 500ms in nanoseconds
#define TRIGGER_COMBO_WINDOW 300000000ULL  // 300ms for double-tap

static HidSixAxisSensorHandle sixaxis_handles[4];
static HidVibrationDeviceHandle vib_handles[2];
static PadState pad;
// Flags to indicate whether motion and vibration devices were successfully initialized
static bool g_motion_available = false;
static bool g_vibration_available = false;

Result input_handler_init(void) {
    Result rc = 0;
    // Ensure handles are zeroed so later calls can check validity
    for (int i = 0; i < 4; ++i) sixaxis_handles[i] = (HidSixAxisSensorHandle){0};
    for (int i = 0; i < 2; ++i) vib_handles[i] = (HidVibrationDeviceHandle){0};
    g_motion_available = false; g_vibration_available = false;
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    // Initialize vibration devices (best-effort)
    rc = hidInitializeVibrationDevices(vib_handles, 2, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    if (R_SUCCEEDED(rc)) g_vibration_available = true; else g_vibration_available = false;

    // Initialize motion sensors (six-axis)
    rc = hidGetSixAxisSensorHandles(sixaxis_handles, 1, HidNpadIdType_Handheld, HidNpadStyleTag_NpadHandheld);
    if (R_SUCCEEDED(rc)) {
        hidStartSixAxisSensor(sixaxis_handles[0]);
        g_motion_available = true;
    } else {
        g_motion_available = false;
    }

    return rc;
}

void input_handler_exit(void) {
    // Stop sensors and vibration if they were started
    if (g_motion_available) {
        hidStopSixAxisSensor(sixaxis_handles[0]);
    }
}

void input_handler_update(InputState* state) {
    padUpdate(&pad);
    u64 kDown = padGetButtonsDown(&pad);
    // Mirror the internal pad state into the caller-provided InputState so callers
    // that use state->pad (e.g., file_explorer) have a valid, initialized PadState.
    if (state) state->pad = pad;
    HidAnalogStickState lStick = padGetStickPos(&pad, 0);  // Left stick
    HidAnalogStickState rStick = padGetStickPos(&pad, 1);  // Right stick

    // Left stick navigation (deadzone)
    if (abs(lStick.y) > 0x4000) {
        state->selection_index += (lStick.y < 0) ? 1 : -1;
    }

    // Right stick scrolling
    if (abs(rStick.y) > 0x4000) {
        state->scroll_offset += (rStick.y < 0) ? 1 : -1;
    }

    // Toggle multi-select on Y
    if (kDown & HidNpadButton_Y) {
        state->multi_select_active = !state->multi_select_active;
    }

    // ZL+ZR double-tap detection
    if ((kDown & (HidNpadButton_ZL | HidNpadButton_ZR)) == (HidNpadButton_ZL | HidNpadButton_ZR)) {
        u64 current_time = armGetSystemTick();
        if (!state->trigger_pressed) {
            state->trigger_pressed = true;
            state->last_trigger_time = current_time;
        } else {
            if (current_time - state->last_trigger_time < TRIGGER_COMBO_WINDOW) {
                // Double-tap detected - caller may handle
                state->trigger_pressed = false;
            } else {
                state->last_trigger_time = current_time;
            }
        }
    }

    // Process motion for shake and tilt
    input_handler_process_motion(state);
}

void input_handler_process_motion(InputState* state) {
    // If motion not available, skip processing
    if (!g_motion_available) return;
    HidSixAxisSensorState sixaxis = {0};
    if (R_SUCCEEDED(hidGetSixAxisSensorStates(sixaxis_handles[0], &sixaxis, 1))) {
        // Compute acceleration magnitude
        float ax = sixaxis.acceleration.x;
        float ay = sixaxis.acceleration.y;
        float az = sixaxis.acceleration.z;
        float accel_magnitude = sqrtf(ax*ax + ay*ay + az*az);

        if (accel_magnitude > SHAKE_THRESHOLD) {
            u64 current_time = armGetSystemTick();
            if (current_time - state->last_shake_time > SHAKE_COOLDOWN) {
                state->last_shake_time = current_time;
                // Provide short rumble as feedback (best-effort)
                HidVibrationValue val = { .freq_low = 160.0f, .freq_high = 320.0f, .amp_low = 0.5f, .amp_high = 0.5f };
                // Only attempt rumble if vibration devices initialized
                if (g_vibration_available) input_handler_rumble_feedback(&val);
            }
        }

        // Tilt angle (degrees)
        state->tilt_angle = atan2f(sixaxis.acceleration.x, sixaxis.acceleration.y) * (180.0f / (float)M_PI);
    }
}

void input_handler_rumble_feedback(const HidVibrationValue* value) {
    // Best-effort: send to initialized vibration handles
    if (!g_vibration_available) return;
    // hidSendVibrationValues expects non-const, so cast away const for API call
    hidSendVibrationValues(vib_handles, (HidVibrationValue*)value, 1);
}

bool input_handler_was_shake_detected(const InputState* state) {
    u64 current_time = armGetSystemTick();
    return (current_time - state->last_shake_time) < SHAKE_COOLDOWN;
}

int input_handler_get_sort_mode(const InputState* state) {
    if (state->tilt_angle < -20.0f) return 0; // by name
    if (state->tilt_angle > 20.0f) return 2;  // by size
    return 1; // by date
}

bool input_handler_check_trigger_combo(InputState* state) {
    u64 current_time = armGetSystemTick();
    bool was_combo = state->trigger_pressed && (current_time - state->last_trigger_time < TRIGGER_COMBO_WINDOW);
    state->trigger_pressed = false;
    return was_combo;
}
