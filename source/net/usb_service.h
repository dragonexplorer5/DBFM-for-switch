#ifndef USB_SERVICE_H
#define USB_SERVICE_H

#include <switch.h>

// Connection states
typedef enum {
    UsbState_Disconnected,
    UsbState_Connected,
    UsbState_Ready,
    UsbState_Error
} UsbState;

// File transfer modes
typedef enum {
    UsbTransfer_Send,
    UsbTransfer_Receive
} UsbTransferMode;

// USB service initialization
Result usb_init(void);
void usb_exit(void);

// Connection management
Result usb_start_service(void);
void usb_stop_service(void);
UsbState usb_get_state(void);

// File operations
Result usb_send_file(const char* local_path, const char* remote_path,
                    void (*progress_callback)(size_t current, size_t total));
Result usb_receive_file(const char* remote_path, const char* local_path,
                       void (*progress_callback)(size_t current, size_t total));

// Remote install operations
Result usb_install_title(const char* remote_path,
                        void (*progress_callback)(size_t current, size_t total));

// Status and error handling
const char* usb_get_error_message(Result rc);
const char* usb_get_state_string(UsbState state);

// Remote command interface
Result usb_send_command(const char* command, char* response, size_t response_size);

#endif // USB_SERVICE_H