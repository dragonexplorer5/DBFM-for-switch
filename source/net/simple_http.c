// simple_http.c - minimal HTTP GET implementation using BSD sockets
// Notes:
// - Supports only plain HTTP (no TLS). HTTPS will return error unless
//   libcurl or mbedTLS support is enabled elsewhere.
// - This is intentionally small and defensive; it reads until the server
//   closes the connection and returns the body (strips headers).

#include "simple_http.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>

// Helper: parse URL of form http://host[:port]/path
static int parse_http_url(const char *url, char **out_host, char **out_port, char **out_path) {
    if (!url) return -1;
    const char *p = url;
    if (strncmp(p, "http://", 7) == 0) p += 7;
    else return -1; // only http supported

    const char *host_start = p;
    const char *path_start = strchr(p, '/');
    const char *host_end = path_start ? path_start : p + strlen(p);

    // look for ':'
    const char *colon = memchr(host_start, ':', host_end - host_start);
    if (colon) {
        *out_host = strndup(host_start, colon - host_start);
        *out_port = strndup(colon + 1, host_end - colon - 1);
    } else {
        *out_host = strndup(host_start, host_end - host_start);
        *out_port = strdup("80");
    }
    if (path_start) *out_path = strdup(path_start);
    else *out_path = strdup("/");
    return 0;
}

int simple_http_get(const char *url, char **out_buf, size_t *out_len) {
    if (!url || !out_buf || !out_len) return -1;

    // Follow up to 4 redirects
    char *current_url = strdup(url);
    int redirects = 0;
    int final_rc = -1;
    while (redirects < 4) {
        char *host = NULL, *port = NULL, *path = NULL;
        if (parse_http_url(current_url, &host, &port, &path) != 0) { free(current_url); return -1; }

        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        int rc = getaddrinfo(host, port, &hints, &res);
        if (rc != 0) {
            free(host); free(port); free(path); free(current_url);
            return -1;
        }

        int sock = -1;
        struct addrinfo *rp;
        for (rp = res; rp != NULL; rp = rp->ai_next) {
            sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sock == -1) continue;
            if (connect(sock, rp->ai_addr, rp->ai_addrlen) == 0) break;
            close(sock); sock = -1;
        }
        freeaddrinfo(res);
        if (sock == -1) { free(host); free(port); free(path); free(current_url); return -1; }

        // Build request
        char req[1024];
        snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\nUser-Agent: DBFM/1.0\r\n\r\n", path, host);

        ssize_t sent = send(sock, req, strlen(req), 0);
        if (sent < 0) { close(sock); free(host); free(port); free(path); free(current_url); return -1; }

        // Read response into growing buffer
        size_t cap = 8192;
        char *buf = malloc(cap);
        if (!buf) { close(sock); free(host); free(port); free(path); free(current_url); return -1; }
        size_t len = 0;
        for (;;) {
            if (len + 4096 > cap) {
                size_t nc = cap * 2;
                char *nb = realloc(buf, nc);
                if (!nb) { free(buf); close(sock); free(host); free(port); free(path); free(current_url); return -1; }
                buf = nb; cap = nc;
            }
            ssize_t r = recv(sock, buf + len, cap - len, 0);
            if (r < 0) { free(buf); close(sock); free(host); free(port); free(path); free(current_url); return -1; }
            if (r == 0) break; // closed
            len += (size_t)r;
        }
        close(sock);

        // Find header/body split: \r\n\r\n
        const char *body = NULL; size_t body_len = 0; size_t header_len = 0;
        for (size_t i = 0; i + 3 < len; ++i) {
            if (buf[i] == '\r' && buf[i+1] == '\n' && buf[i+2] == '\r' && buf[i+3] == '\n') {
                body = buf + i + 4;
                header_len = i + 4;
                body_len = len - header_len;
                break;
            }
        }
        if (!body) {
            // no header separation; return whole buffer
            *out_buf = buf; *out_len = len;
            free(host); free(port); free(path); free(current_url);
            return 0;
        }

        // Parse status code from first line
        int status = 0;
        char status_line[128] = {0};
        size_t i = 0;
        while (i < header_len && i < sizeof(status_line)-1 && buf[i] != '\r') { status_line[i] = buf[i]; i++; }
        status = 0; sscanf(status_line, "HTTP/%*s %d", &status);

        // Parse headers for Transfer-Encoding and Location
        int is_chunked = 0;
        char *location = NULL;
        const char *hpos = buf;
        while (hpos < buf + header_len) {
            const char *line_end = strstr(hpos, "\r\n");
            if (!line_end) break;
            size_t llen = line_end - hpos;
            if (llen == 0) break; // end headers
            if (strncasecmp(hpos, "Transfer-Encoding:", 18) == 0) {
                if (strncasecmp(hpos+18, " chunked", 8) == 0) is_chunked = 1;
                else if (strstr(hpos+18, "chunked")) is_chunked = 1;
            }
            if (strncasecmp(hpos, "Location:", 9) == 0) {
                // store location
                size_t off = 9; while (off < llen && (hpos[off] == ' ' || hpos[off] == '\t')) off++;
                location = strndup(hpos + off, llen - off);
            }
            hpos = line_end + 2;
        }

        // Handle redirects
        if ((status == 301 || status == 302 || status == 303 || status == 307 || status == 308) && location) {
            // follow redirect
            free(buf);
            free(host); free(port); free(path);
            free(current_url);
            current_url = location; // already strdup'd
            redirects++;
            continue;
        }

        // Handle chunked transfer encoding
        if (is_chunked) {
            // decode chunked body
            size_t out_cap = 1024;
            char *out = malloc(out_cap);
            size_t out_len_local = 0;
            const char *p = body;
            const char *endp = buf + len;
            while (p < endp) {
                // read chunk size line
                const char *ln = strstr(p, "\r\n");
                if (!ln) break;
                char numbuf[32]; size_t nlen = ln - p; if (nlen >= sizeof(numbuf)) nlen = sizeof(numbuf)-1;
                memcpy(numbuf, p, nlen); numbuf[nlen] = '\0';
                unsigned int chunk_size = 0; sscanf(numbuf, "%x", &chunk_size);
                p = ln + 2;
                if (chunk_size == 0) break;
                if (out_len_local + chunk_size + 1 > out_cap) {
                    size_t nc = out_cap;
                    while (out_len_local + chunk_size + 1 > nc) nc *= 2;
                    char *nb = realloc(out, nc);
                    if (!nb) { free(out); free(buf); free(host); free(port); free(path); free(current_url); return -1; }
                    out = nb; out_cap = nc;
                }
                if (p + chunk_size > endp) break;
                memcpy(out + out_len_local, p, chunk_size);
                out_len_local += chunk_size;
                p += chunk_size;
                // skip CRLF
                if (p + 2 <= endp && p[0] == '\r' && p[1] == '\n') p += 2;
            }
            out[out_len_local] = '\0';
            *out_buf = out; *out_len = out_len_local;
            free(buf); free(host); free(port); free(path); free(current_url);
            final_rc = 0; break;
        } else {
            // normal body
            char *b = malloc(body_len + 1);
            if (!b) { free(buf); free(host); free(port); free(path); free(current_url); return -1; }
            memcpy(b, body, body_len);
            b[body_len] = '\0';
            *out_buf = b; *out_len = body_len;
            free(buf); free(host); free(port); free(path); free(current_url);
            final_rc = 0; break;
        }
    }
    return final_rc;
}
