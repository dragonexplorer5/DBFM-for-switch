/* Minimal curl stubs to avoid requiring libcurl at build time */

typedef int CURLcode;
#define CURLE_OK 0

/* Provide minimal stub implementations used by the project */
CURLcode curl_global_init(long flags) { (void)flags; return CURLE_OK; }
void curl_global_cleanup(void) { /* no-op */ }
