/* input_handler.h - single clean header */

#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <switch.h>
#include <stdbool.h>

// Input state used across UI and features
typedef struct {
    bool multi_select_active;
    float tilt_angle;
    u64 last_shake_time;
    u64 last_trigger_time;
    bool trigger_pressed;
    int scroll_offset;
    int selection_index;
    HidSixAxisSensorHandle sixaxis_handles[4];
    bool sixaxis_enabled;
    // Pad state used by the global pad in input handler implementation
    PadState pad;
} InputState;

// Initialize input handling and sensors
Result input_handler_init(void);
// Clean up input handling
void input_handler_exit(void);
// Update input state and handle all controls
void input_handler_update(InputState* state);
// Handle gyro/accelerometer for shake and tilt
void input_handler_process_motion(InputState* state);
// Provide HD rumble feedback
void input_handler_rumble_feedback(const HidVibrationValue* value);
// Check if shake gesture was detected
bool input_handler_was_shake_detected(const InputState* state);
// Get current tilt-based sort mode
int input_handler_get_sort_mode(const InputState* state);
// Check if ZL+ZR double tap occurred
bool input_handler_check_trigger_combo(InputState* state);

#endif // INPUT_HANDLER_H
