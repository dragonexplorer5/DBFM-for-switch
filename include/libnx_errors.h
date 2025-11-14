#ifndef LIBNX_ERRORS_H
#define LIBNX_ERRORS_H

// Custom error codes for libnx compatibility
// These map the project-expected LibnxError_* names to integer codes so
// older code that used MAKERESULT(Module_Libnx, <code>) can compile.
#define LibnxError_NotAllowed 100
#define LibnxError_BadMagic 101
#define LibnxError_InvalidKey 102
#define LibnxError_VerificationFailed 103
#define LibnxError_MissingTicket 104
#define LibnxError_RequestCanceled 105

// Common/expected error names used in the project
#define LibnxError_BadInput 110
#define LibnxError_NotInitialized 111
#define LibnxError_NotFound 112
#define LibnxError_IoError 113
#define LibnxError_OutOfMemory 114

#endif // LIBNX_ERRORS_H