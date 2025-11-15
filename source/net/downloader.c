/* downloader.c - simple file/romfs loader with optional libcurl support.
 * This implementation will read local files (romfs or file paths). If libcurl
 * support is required, you can add it later (and update the Makefile to link
 * against libcurl). For now, HTTP(S) URLs are not fetched and will return -1.
 */

#include "downloader.h"
#include "../ui/ui_data.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <errno.h>
#ifdef USE_LIBCURL
#include <curl/curl.h>
#endif
#include "simple_http.h"

#ifdef USE_MBEDTLS
#include <mbedtls/net_sockets.h>
#include <mbedtls/ssl.h>
#include <mbedtls/entropy.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/error.h>
#include <mbedtls/x509_crt.h>
#endif

#ifdef USE_LIBCURL
struct mem_block { char *data; size_t size; };
static size_t downloader_curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t realsize = size * nmemb;
    struct mem_block *m = (struct mem_block*)userdata;
    char *ptr2 = (char*)realloc(m->data, m->size + realsize + 1);
    if (!ptr2) return 0; // out of memory
    m->data = ptr2;
    memcpy(&(m->data[m->size]), ptr, realsize);
    m->size += realsize;
    m->data[m->size] = 0;
    return realsize;
}
#endif

#ifdef USE_LIBCURL
// curl progress callback wrapper
struct curl_progress_data { download_progress_cb cb; FILE *f; char label[128]; };
static int curl_progress_func(void *p, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    struct curl_progress_data *pd = (struct curl_progress_data*)p;
    if (pd && pd->cb) {
        pd->cb("Downloading", (size_t)dlnow, (size_t)dltotal);
        // Update UI downloads queue
        if (pd->label && pd->label[0]) {
            int pct = 0; if (dltotal > 0) pct = (int)((dlnow * 100) / dltotal);
            ui_downloads_push_update(pd->label, pct);
        }
    }
    return 0;
}
// Older curl progress function (double args)
static int curl_progress_func_old(void *p, double dltotal, double dlnow, double ultotal, double ulnow) {
    struct curl_progress_data *pd = (struct curl_progress_data*)p;
    if (pd && pd->cb) {
        pd->cb("Downloading", (size_t)dlnow, (size_t)dltotal);
        if (pd->label && pd->label[0]) {
            int pct = 0; if (dltotal > 0.0) pct = (int)((dlnow * 100.0) / dltotal);
            ui_downloads_push_update(pd->label, pct);
        }
    }
    return 0;
}
#endif

// Cancellation flag for current download.
static volatile int downloader_cancel_flag = 0;

void downloader_cancel_current(void) {
    downloader_cancel_flag = 1;
}

#ifdef USE_LIBCURL
// curl write callback: write data directly to FILE* passed via userdata
static size_t curl_write_to_file_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t realsize = size * nmemb;
    if (downloader_cancel_flag) return 0; // returning 0 aborts curl transfer
    FILE *f = (FILE*)userdata;
    size_t written = fwrite(ptr, 1, realsize, f);
    return written;
}
#endif

int download_url_to_memory(const char *url, char **out_buf, size_t *out_len) {
    if (!url || !out_buf || !out_len) return -1;
    // If URL starts with http/https, attempt to use libcurl when enabled,
    // otherwise fall back to a minimal HTTP client for plain http:// URLs.
    if (strncmp(url, "http://", 7) == 0 || strncmp(url, "https://", 8) == 0) {
#ifdef USE_LIBCURL
        // simple curl-based fetch into memory
        struct curl_slist *headers = NULL;
        CURL *curl = curl_easy_init();
        if (!curl) return -1;

        struct mem_block chunk = {0};

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "DBFM/1.0");
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, downloader_curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

        CURLcode res = curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        if (res != CURLE_OK) {
            if (chunk.data) free(chunk.data);
            return -1;
        }
        *out_buf = chunk.data;
        *out_len = chunk.size;
        return 0;
#else
        // plain HTTP can be handled by our simple client; HTTPS is unsupported
        if (strncmp(url, "http://", 7) == 0) {
            return simple_http_get(url, out_buf, out_len);
        }
        return -1; // HTTPS not available without libcurl/mbedtls
#endif
    }

    // Allow romfs: or direct file path
    const char *path = url;
    if (strncmp(url, "romfs:/", 7) == 0) {
        path = url + 7; // skip romfs:/ prefix
    }

    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    char *buf = malloc(len + 1);
    if (!buf) { fclose(f); return -1; }
    if (fread(buf, 1, len, f) != (size_t)len) { free(buf); fclose(f); return -1; }
    buf[len] = '\0';
    fclose(f);

    *out_buf = buf;
    *out_len = (size_t)len;
    return 0;
}

// Stream the given URL directly to a file on disk and call progress_cb.
// Returns 0 on success.
int download_url_to_file(const char *url, const char *out_path, download_progress_cb progress_cb) {
    if (!url || !out_path) return -1;

    // Derive short filename label for UI use
    const char *slash_name = strrchr(out_path, '/');
    const char *fname = slash_name ? slash_name + 1 : out_path;

    // If libcurl is enabled, use it to stream directly to file (handles HTTPS too)
#ifdef USE_LIBCURL
    CURL *curl = curl_easy_init();
    if (!curl) return -1;

    FILE *f = fopen(out_path, "wb");
    if (!f) { curl_easy_cleanup(curl); return -1; }

    struct curl_progress_data prog; memset(&prog, 0, sizeof(prog)); prog.cb = progress_cb; prog.f = f;
    // Use the common fname derived above
    snprintf(prog.label, sizeof(prog.label), "%s", fname ? fname : "download");
    // Add initial UI entry
    ui_downloads_push_update(fname, 0);

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "DBFM/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

    // set write callback to stream to file
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_to_file_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);

#if LIBCURL_VERSION_NUM >= 0x072000
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &prog);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress_func);
#else
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, &prog);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func_old);
#endif

    // Reset cancel flag
    downloader_cancel_flag = 0;

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);
    fclose(f);

    // Ensure UI reflects completion/failure
    if (prog.label && prog.label[0]) {
        if (res == CURLE_OK) ui_downloads_remove(prog.label);
        else ui_downloads_remove(prog.label);
    }

    if (res != CURLE_OK) {
        // remove partial file
        unlink(out_path);
        ui_clear_task();
        return -1;
    }
    ui_clear_task();
    return 0;
#elif defined(USE_MBEDTLS)
    // mbedTLS-backed HTTPS path (when USE_MBEDTLS is enabled)
    if (strncmp(url, "https://", 8) == 0) {
        const char *p = url + 8;
        const char *path = strchr(p, '/');
        char host[256] = {0};
        if (!path) { strncpy(host, p, sizeof(host)-1); path = "/"; }
        else { size_t hlen = (size_t)(path - p); if (hlen >= sizeof(host)) hlen = sizeof(host)-1; memcpy(host, p, hlen); host[hlen] = '\0'; }
        const char *port = "443";

    FILE *f = fopen(out_path, "wb"); if (!f) return -1;
    ui_downloads_push_update(fname, 0);

        int rc = -1;
        mbedtls_net_context server_fd; mbedtls_ssl_context ssl; mbedtls_ssl_config conf;
        mbedtls_entropy_context entropy; mbedtls_ctr_drbg_context ctr_drbg; mbedtls_x509_crt cacert;
        const char *pers = "dbfm_tls";

        mbedtls_net_init(&server_fd);
        mbedtls_ssl_init(&ssl);
        mbedtls_ssl_config_init(&conf);
        mbedtls_ctr_drbg_init(&ctr_drbg);
        mbedtls_entropy_init(&entropy);
        mbedtls_x509_crt_init(&cacert);

        if ((rc = mbedtls_ctr_drbg_seed(&ctr_drbg, mbedtls_entropy_func, &entropy,
                        (const unsigned char*)pers, strlen(pers))) != 0) goto mbed_cleanup;

        if ((rc = mbedtls_net_connect(&server_fd, host, port, MBEDTLS_NET_PROTO_TCP)) != 0) goto mbed_cleanup;

        if ((rc = mbedtls_ssl_config_defaults(&conf, MBEDTLS_SSL_IS_CLIENT, MBEDTLS_SSL_TRANSPORT_STREAM, MBEDTLS_SSL_PRESET_DEFAULT)) != 0) goto mbed_cleanup;
        mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE); // don't verify by default on Switch
        mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &ctr_drbg);

        if ((rc = mbedtls_ssl_setup(&ssl, &conf)) != 0) goto mbed_cleanup;
        mbedtls_ssl_set_hostname(&ssl, host);
        mbedtls_ssl_set_bio(&ssl, &server_fd, mbedtls_net_send, mbedtls_net_recv, NULL);

        while ((rc = mbedtls_ssl_handshake(&ssl)) != 0) {
            if (rc != MBEDTLS_ERR_SSL_WANT_READ && rc != MBEDTLS_ERR_SSL_WANT_WRITE) goto mbed_cleanup;
        }

        char req[1024]; snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: DBFM/1.0\r\n\r\n", path, host);
        size_t to_send = strlen(req); size_t sent = 0;
        while (sent < to_send) {
            int n = mbedtls_ssl_write(&ssl, (const unsigned char*)req + sent, to_send - sent);
            if (n <= 0) { rc = n; goto mbed_cleanup; }
            sent += (size_t)n;
        }

        unsigned char buf[8192]; unsigned char header_buf[16384]; size_t header_len = 0; int header_done = 0; size_t total_written = 0;
        while (1) {
            if (downloader_cancel_flag) { rc = -1; break; }
            int r = mbedtls_ssl_read(&ssl, buf, sizeof(buf));
            if (r == MBEDTLS_ERR_SSL_WANT_READ || r == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
            if (r <= 0) break;
            size_t got = (size_t)r;
            if (!header_done) {
                if (header_len + got > sizeof(header_buf)) { rc = -1; break; }
                memcpy(header_buf + header_len, buf, got); header_len += got;
                for (size_t i = 0; i + 3 < header_len; ++i) {
                    if (header_buf[i]=='\r' && header_buf[i+1]=='\n' && header_buf[i+2]=='\r' && header_buf[i+3]=='\n') {
                        size_t body_off = i + 4; size_t body_len = header_len - body_off;
                        if (body_len) { fwrite(header_buf + body_off, 1, body_len, f); total_written += body_len; }
                        header_done = 1; header_len = 0; break;
                    }
                }
                continue;
            } else {
                fwrite(buf, 1, got, f); total_written += got;
            }
            if (progress_cb) progress_cb("Downloading", total_written, 0);
            // update UI with unknown total (0)
            if (progress_cb && fname) {
                // percent unknown => report 0.. we still push an entry
                ui_downloads_push_update(fname, 0);
            }
        }

    if (!downloader_cancel_flag) rc = 0;

    mbed_cleanup:
        mbedtls_ssl_close_notify(&ssl);
        mbedtls_net_free(&server_fd);
        mbedtls_x509_crt_free(&cacert);
        mbedtls_ssl_free(&ssl);
        mbedtls_ssl_config_free(&conf);
        mbedtls_ctr_drbg_free(&ctr_drbg);
        mbedtls_entropy_free(&entropy);
        fclose(f);
        if (rc != 0) { unlink(out_path); ui_downloads_remove(fname); ui_clear_task(); return -1; }
        ui_downloads_remove(fname); ui_clear_task();
        return 0;
    }
    // fall through to HTTP socket path below if not https://
#else
    // No libcurl: support plain http:// streaming directly via sockets
    if (strncmp(url, "http://", 7) != 0) return -1; // HTTPS not supported

    // Simple parse: extract host, optional port, and path
    const char *p = url + 7; // skip http://
    const char *slash = strchr(p, '/');
    size_t host_len = slash ? (size_t)(slash - p) : strlen(p);
    char host[256] = {0};
    char portstr[8] = "80";
    const char *portptr = memchr(p, ':', host_len);
    if (portptr) {
        size_t hlen = (size_t)(portptr - p);
        if (hlen >= sizeof(host)) hlen = sizeof(host)-1;
        memcpy(host, p, hlen); host[hlen] = '\0';
        size_t plen = host_len - hlen - 1;
        if (plen >= sizeof(portstr)) plen = sizeof(portstr)-1;
        memcpy(portstr, portptr+1, plen); portstr[plen] = '\0';
    } else {
        if (host_len >= sizeof(host)) host_len = sizeof(host)-1;
        memcpy(host, p, host_len); host[host_len] = '\0';
    }
    const char *path = slash ? slash : "/";

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints)); hints.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, portstr, &hints, &res) != 0) return -1;

    int sock = -1; struct addrinfo *rp;
    for (rp = res; rp; rp = rp->ai_next) {
        sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sock < 0) continue;
        if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
        close(sock); sock = -1;
    }
    freeaddrinfo(res);
    if (sock < 0) return -1;

    char req[1024]; snprintf(req, sizeof(req), "GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\nUser-Agent: DBFM/1.0\r\n\r\n", path, host);
    if (send(sock, req, strlen(req), 0) < 0) { close(sock); return -1; }

    FILE *f = fopen(out_path, "wb"); if (!f) { close(sock); return -1; }
    ui_downloads_push_update(fname, 0);

    // Use common fname derived at function top
    (void)0;

    // Read headers first
    char header_buf[16384]; size_t header_len = 0; int header_done = 0;
    while (!header_done) {
        ssize_t r = recv(sock, header_buf + header_len, sizeof(header_buf) - header_len, 0);
        if (r <= 0) { fclose(f); close(sock); return -1; }
        header_len += (size_t)r;
        // look for \r\n\r\n
        for (size_t i = 0; i + 3 < header_len; ++i) {
            if (header_buf[i] == '\r' && header_buf[i+1] == '\n' && header_buf[i+2] == '\r' && header_buf[i+3] == '\n') {
                // write any initial body data after header
                size_t hdr_end = i + 4;
                size_t body_bytes = header_len - hdr_end;
                if (body_bytes) fwrite(header_buf + hdr_end, 1, body_bytes, f);
                // parse headers for content-length and chunked
                int is_chunked = 0; size_t content_length = 0;
                header_buf[header_len] = '\0';
                char *hpos = header_buf;
                while (hpos < header_buf + hdr_end) {
                    char *line = hpos;
                    char *line_end = strstr(line, "\r\n"); if (!line_end) break;
                    *line_end = '\0';
                    if (strncasecmp(line, "Content-Length:", 15) == 0) content_length = (size_t)atoll(line + 15);
                    if (strncasecmp(line, "Transfer-Encoding:", 18) == 0 && strstr(line+18, "chunked")) is_chunked = 1;
                    hpos = line_end + 2;
                }
                // If chunked, decode on the fly
                    if (is_chunked) {
                    // We already wrote any partial chunk body bytes; now iterate reading chunks
                    char chunkline[64]; size_t partial_bytes = body_bytes; // may be portion of chunk
                    // Use a simple stateful reader: put back any unread body bytes to buffer processing
                    // For simplicity, assume header_end aligns to chunk boundary rarely; we will handle by feeding remaining bytes into decoding loop
                    // Build a small buffer for chunk processing
                    size_t bufcap = 8192; char *buf = malloc(bufcap); size_t buflen = 0;
                    if (body_bytes) { buflen = body_bytes; if (buflen > bufcap) buflen = bufcap; memcpy(buf, header_buf + hdr_end, buflen); }
                    int done = 0;
                    while (!done) {
                        // ensure we have a line for chunk size
                        char *lnpos = memchr(buf, '\n', buflen);
                        while (!lnpos) {
                            // read more data
                            if (bufcap - buflen < 4096) { bufcap *= 2; char *nb = realloc(buf, bufcap); if (!nb) { free(buf); fclose(f); close(sock); return -1; } buf = nb; }
                            ssize_t r = recv(sock, buf + buflen, bufcap - buflen, 0);
                            if (r <= 0) { free(buf); fclose(f); close(sock); return -1; }
                            buflen += (size_t)r;
                            lnpos = memchr(buf, '\n', buflen);
                        }
                        // read chunk size line
                        size_t linelen = (size_t)(lnpos - buf) + 1; // include newline
                        char numbuf[32]; size_t copylen = linelen < sizeof(numbuf)-1 ? linelen : sizeof(numbuf)-1;
                        memcpy(numbuf, buf, copylen); numbuf[copylen] = '\0';
                        unsigned int chunk_size = 0; sscanf(numbuf, "%x", &chunk_size);
                        // remove size line from buffer
                        size_t remaining = buflen - linelen;
                        memmove(buf, buf + linelen, remaining); buflen = remaining;
                        if (chunk_size == 0) { done = 1; break; }
                        // ensure we have chunk_size + 2 (CRLF)
                        while (buflen < chunk_size + 2) {
                            if (bufcap - buflen < 4096) { bufcap *= 2; char *nb = realloc(buf, bufcap); if (!nb) { free(buf); fclose(f); close(sock); return -1; } buf = nb; }
                            ssize_t r = recv(sock, buf + buflen, bufcap - buflen, 0);
                            if (r <= 0) { free(buf); fclose(f); close(sock); return -1; }
                            buflen += (size_t)r;
                        }
                        // write chunk_size bytes
                        fwrite(buf, 1, chunk_size, f);
                        // drop chunk and trailing CRLF
                        remaining = buflen - (chunk_size + 2);
                        memmove(buf, buf + chunk_size + 2, remaining); buflen = remaining;
                        if (progress_cb) progress_cb("Downloading", 0, 0);
                        if (fname) ui_downloads_push_update(fname, 0);
                    }
                    free(buf);
                    fclose(f); close(sock);
                    return 0;
                } else {
                    // Not chunked: continue streaming remaining content-length or until close
                    size_t written = body_bytes;
                    if (progress_cb) progress_cb("Downloading", written, content_length);
                    char tmp[4096]; ssize_t r;
                        while ((r = recv(sock, tmp, sizeof(tmp), 0)) > 0) {
                        fwrite(tmp, 1, (size_t)r, f);
                        written += (size_t)r;
                        if (progress_cb) progress_cb("Downloading", written, content_length);
                        if (content_length > 0 && fname) {
                            int pct = (int)((written * 100) / content_length);
                            ui_downloads_push_update(fname, pct);
                        } else if (fname) {
                            ui_downloads_push_update(fname, 0);
                        }
                    }
                    fclose(f); close(sock);
                    ui_downloads_remove(fname); ui_clear_task();
                    return 0;
                }
            }
        }
    }
#endif
    return -1;
}

// downloader_cancel_current implemented above (sets a cancel flag)
