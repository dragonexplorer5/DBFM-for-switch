#include "usb_service.h"
#include "verify.h"
#include "crypto.h"
#include "fs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USB_BUFFER_SIZE (1 * 1024 * 1024)  // 1MB transfer buffer
#define USB_VENDOR_ID 0x057E               // Nintendo
#define USB_PRODUCT_ID 0x3000              // Switch
#define USB_INTERFACE_CLASS 0xFF           // Vendor-specific
#define USB_TIMEOUT 1000                   // 1 second timeout

// Protocol commands
#define CMD_HELLO "HELLO"
#define CMD_BYE "BYE"
#define CMD_LIST "LIST"
#define CMD_SEND "SEND"
#define CMD_RECV "RECV"
#define CMD_INST "INST"

static UsbState s_state = UsbState_Disconnected;
static UsbDsInterface* s_interface = NULL;
static UsbDsEndpoint* s_endpoint_in = NULL;
static UsbDsEndpoint* s_endpoint_out = NULL;
static bool s_initialized = false;

Result usb_init(void) {
    if (s_initialized) return 0;

    Result rc = usbDsInitialize();
    if (R_FAILED(rc)) return rc;

    s_initialized = true;
    return 0;
}

void usb_exit(void) {
    if (!s_initialized) return;

    usb_stop_service();
    usbDsExit();
    s_initialized = false;
}

static Result _usb_setup_interface(void) {
    if (!s_initialized) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);

    struct usb_interface_descriptor interface_descriptor = {
        .bLength = USB_DT_INTERFACE_SIZE,
        .bDescriptorType = USB_DT_INTERFACE,
        .bInterfaceNumber = 0,
        .bAlternateSetting = 0,
        .bNumEndpoints = 2,
        .bInterfaceClass = USB_INTERFACE_CLASS,
        .bInterfaceSubClass = 0x00,
        .bInterfaceProtocol = 0x00,
        .iInterface = 0,
    };

    Result rc = usbDsRegisterInterface(&s_interface);
    if (R_FAILED(rc)) return rc;

    rc = usbDsInterface_AppendConfigurationData(s_interface, 
                                              &interface_descriptor, 
                                              USB_DT_INTERFACE_SIZE);
    if (R_FAILED(rc)) {
        usbDsInterface_Close(s_interface);
        s_interface = NULL;
        return rc;
    }

    struct usb_endpoint_descriptor endpoint_descriptor_in = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_IN,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x200,
        .bInterval = 0,
    };

    rc = usbDsInterface_AppendConfigurationData(s_interface,
                                              &endpoint_descriptor_in,
                                              USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) {
        usbDsInterface_Close(s_interface);
        s_interface = NULL;
        return rc;
    }

    struct usb_endpoint_descriptor endpoint_descriptor_out = {
        .bLength = USB_DT_ENDPOINT_SIZE,
        .bDescriptorType = USB_DT_ENDPOINT,
        .bEndpointAddress = USB_ENDPOINT_OUT,
        .bmAttributes = USB_TRANSFER_TYPE_BULK,
        .wMaxPacketSize = 0x200,
        .bInterval = 0,
    };

    rc = usbDsInterface_AppendConfigurationData(s_interface,
                                              &endpoint_descriptor_out,
                                              USB_DT_ENDPOINT_SIZE);
    if (R_FAILED(rc)) {
        usbDsInterface_Close(s_interface);
        s_interface = NULL;
        return rc;
    }

    rc = usbDsInterface_RegisterEndpoint(s_interface, &s_endpoint_in,
                                       endpoint_descriptor_in.bEndpointAddress);
    if (R_FAILED(rc)) {
        usbDsInterface_Close(s_interface);
        s_interface = NULL;
        return rc;
    }

    rc = usbDsInterface_RegisterEndpoint(s_interface, &s_endpoint_out,
                                       endpoint_descriptor_out.bEndpointAddress);
    if (R_FAILED(rc)) {
        usbDsEndpoint_Close(s_endpoint_in);
        usbDsInterface_Close(s_interface);
        s_interface = NULL;
        s_endpoint_in = NULL;
        return rc;
    }

    return 0;
}

Result usb_start_service(void) {
    if (!s_initialized) return MAKERESULT(Module_Libnx, LibnxError_NotInitialized);
    if (s_state != UsbState_Disconnected) return 0;

    Result rc = _usb_setup_interface();
    if (R_FAILED(rc)) return rc;

    // Enable interface
    rc = usbDsInterface_Enable(s_interface);
    if (R_FAILED(rc)) {
        usb_stop_service();
        return rc;
    }

    s_state = UsbState_Connected;

    // Send hello command to verify connection
    char response[256];
    rc = usb_send_command(CMD_HELLO, response, sizeof(response));
    if (R_SUCCEEDED(rc) && strcmp(response, "OK") == 0) {
        s_state = UsbState_Ready;
    }

    return rc;
}

void usb_stop_service(void) {
    if (s_state == UsbState_Disconnected) return;

    if (s_state == UsbState_Ready) {
        usb_send_command(CMD_BYE, NULL, 0);
    }

    if (s_endpoint_in) {
        usbDsEndpoint_Close(s_endpoint_in);
        s_endpoint_in = NULL;
    }

    if (s_endpoint_out) {
        usbDsEndpoint_Close(s_endpoint_out);
        s_endpoint_out = NULL;
    }

    if (s_interface) {
        usbDsInterface_Disable(s_interface);
        usbDsInterface_Close(s_interface);
        s_interface = NULL;
    }

    s_state = UsbState_Disconnected;
}

UsbState usb_get_state(void) {
    return s_state;
}

Result usb_send_command(const char* command, char* response, size_t response_size) {
    if (!command || s_state != UsbState_Ready) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    size_t command_len = strlen(command);
    u32 transferred = 0;
    Result rc = usbDsEndpoint_PostBuffer(s_endpoint_out, command, command_len, &transferred);
    if (R_FAILED(rc)) return rc;

    if (response && response_size > 0) {
        rc = usbDsEndpoint_PostBuffer(s_endpoint_in, response, response_size - 1, &transferred);
        if (R_SUCCEEDED(rc)) {
            response[transferred] = '\0';
        }
    }

    return rc;
}

static Result _usb_transfer_file(const char* local_path, const char* remote_path,
                               UsbTransferMode mode,
                               void (*progress_callback)(size_t current, size_t total)) {
    if (!local_path || !remote_path || s_state != UsbState_Ready) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char command[1024];
    char response[256];
    snprintf(command, sizeof(command), "%s %s", 
             mode == UsbTransfer_Send ? CMD_SEND : CMD_RECV,
             remote_path);

    Result rc = usb_send_command(command, response, sizeof(response));
    if (R_FAILED(rc)) return rc;

    if (strcmp(response, "OK") != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    FILE* f = fopen(local_path, mode == UsbTransfer_Send ? "rb" : "wb");
    if (!f) {
        return MAKERESULT(Module_Libnx, LibnxError_NotFound);
    }

    u8* buffer = malloc(USB_BUFFER_SIZE);
    if (!buffer) {
        fclose(f);
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    size_t total_size = 0;
    if (mode == UsbTransfer_Send) {
        fseek(f, 0, SEEK_END);
        total_size = ftell(f);
        fseek(f, 0, SEEK_SET);
    } else {
        sscanf(response + 3, "%zu", &total_size);
    }

    size_t transferred = 0;
    rc = 0;

    while (transferred < total_size) {
        size_t chunk_size = total_size - transferred;
        if (chunk_size > USB_BUFFER_SIZE) chunk_size = USB_BUFFER_SIZE;

        u32 xfer = 0;
        if (mode == UsbTransfer_Send) {
            size_t read = fread(buffer, 1, chunk_size, f);
            if (read != chunk_size) {
                rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
                break;
            }
            rc = usbDsEndpoint_PostBuffer(s_endpoint_out, buffer, chunk_size, &xfer);
        } else {
            rc = usbDsEndpoint_PostBuffer(s_endpoint_in, buffer, chunk_size, &xfer);
            if (R_SUCCEEDED(rc)) {
                if (fwrite(buffer, 1, xfer, f) != xfer) {
                    rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
                    break;
                }
            }
        }

        if (R_FAILED(rc)) break;

        transferred += xfer;
        if (progress_callback) {
            progress_callback(transferred, total_size);
        }
    }

    free(buffer);
    fclose(f);

    if (R_SUCCEEDED(rc)) {
        rc = usb_send_command("DONE", response, sizeof(response));
    }

    return rc;
}

Result usb_send_file(const char* local_path, const char* remote_path,
                    void (*progress_callback)(size_t current, size_t total)) {
    return _usb_transfer_file(local_path, remote_path, UsbTransfer_Send, progress_callback);
}

Result usb_receive_file(const char* remote_path, const char* local_path,
                       void (*progress_callback)(size_t current, size_t total)) {
    return _usb_transfer_file(local_path, remote_path, UsbTransfer_Receive, progress_callback);
}

Result usb_install_title(const char* remote_path,
                        void (*progress_callback)(size_t current, size_t total)) {
    if (!remote_path || s_state != UsbState_Ready) {
        return MAKERESULT(Module_Libnx, LibnxError_BadInput);
    }

    char command[1024];
    char response[256];
    snprintf(command, sizeof(command), "%s %s", CMD_INST, remote_path);

    Result rc = usb_send_command(command, response, sizeof(response));
    if (R_FAILED(rc)) return rc;

    if (strncmp(response, "OK ", 3) != 0) {
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    size_t total_size = 0;
    sscanf(response + 3, "%zu", &total_size);

    u8* buffer = malloc(USB_BUFFER_SIZE);
    if (!buffer) {
        return MAKERESULT(Module_Libnx, LibnxError_OutOfMemory);
    }

    char temp_path[FS_MAX_PATH];
    snprintf(temp_path, sizeof(temp_path), "sdmc:/temp/install_%lx.nsp",
             armGetSystemTick());

    FILE* f = fopen(temp_path, "wb");
    if (!f) {
        free(buffer);
        return MAKERESULT(Module_Libnx, LibnxError_IoError);
    }

    size_t transferred = 0;
    while (transferred < total_size) {
        size_t chunk_size = total_size - transferred;
        if (chunk_size > USB_BUFFER_SIZE) chunk_size = USB_BUFFER_SIZE;

        u32 xfer = 0;
        rc = usbDsEndpoint_PostBuffer(s_endpoint_in, buffer, chunk_size, &xfer);
        if (R_FAILED(rc)) break;

        if (fwrite(buffer, 1, xfer, f) != xfer) {
            rc = MAKERESULT(Module_Libnx, LibnxError_IoError);
            break;
        }

        transferred += xfer;
        if (progress_callback) {
            progress_callback(transferred, total_size);
        }
    }

    fclose(f);
    free(buffer);

    if (R_SUCCEEDED(rc)) {
        // Verify downloaded NSP
        NspVerifyResult verify_result;
        rc = verify_nsp_file(temp_path, &verify_result);
        if (R_SUCCEEDED(rc)) {
            // TODO: Install NSP using nsp_manager
            // rc = nsp_install(temp_path);
        }
        verify_free_nsp_result(&verify_result);
    }

    remove(temp_path);
    return rc;
}

const char* usb_get_error_message(Result rc) {
    if (R_SUCCEEDED(rc)) return "Success";

    switch (rc) {
        case MAKERESULT(Module_Libnx, LibnxError_NotInitialized):
            return "USB service not initialized";
        case MAKERESULT(Module_Libnx, LibnxError_BadInput):
            return "Invalid input parameters";
        case MAKERESULT(Module_Libnx, LibnxError_NotFound):
            return "File not found";
        case MAKERESULT(Module_Libnx, LibnxError_IoError):
            return "I/O error during transfer";
        case MAKERESULT(Module_Libnx, LibnxError_OutOfMemory):
            return "Out of memory";
        default:
            return "Unknown error";
    }
}

const char* usb_get_state_string(UsbState state) {
    switch (state) {
        case UsbState_Disconnected: return "Disconnected";
        case UsbState_Connected: return "Connected";
        case UsbState_Ready: return "Ready";
        case UsbState_Error: return "Error";
        default: return "Unknown";
    }
}