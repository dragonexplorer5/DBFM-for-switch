#include "install.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <ctype.h>
#include <time.h>
#ifdef USE_LIBCURL
#include <curl/curl.h>
#endif

// Edit this list to change which homebrew are shown (no CFW packs included)
InstallItem g_candidates[] = {
    { "hbmenu", "https://github.com/switchbrew/nx-hbmenu/releases/download/v3.6.0/nx-hbmenu_v3.6.0.zip", "Homebrew Menu", "Homebrew Menu", false },
    { "nx-shell", "https://github.com/joel16/NX-Shell/releases/download/4.01/NX-Shell.nro", "ファイルシェル", "File Shell", false },
    { "Checkpoint", "https://github.com/BernardoGiordano/Checkpoint/releases/download/v3.10.1/Checkpoint.nro", "セーブマネージャー", "Save Manager", false },
    { "Goldleaf", "https://github.com/XorTroll/Goldleaf/releases/download/1.1.1/Goldleaf.nro", "インストーラ/ファイラー", "Installer/File manager", false },
    { "Tinfoil", "https://tinfoil.media/repo/Tinfoil%20Self%20Installer%20%5B050000BADDAD0000%5D%5B20.0%5D%5Bv2%5D.zip", "パッケージ管理", "Package manager", false },
    { "Tinfoil Applet version", "https://tinfoil.media/repo/Tinfoil%20Applet%20Mode%20%5B20.0%5D%5Bv2%5D.zip", "パッケージ管理 (アプレット版)", "Package manager (applet mode)", false },
    { "RetroArch", "https://github.com/libretro/RetroArch/releases/download/v1.21.0/retroarch-sourceonly-1.21.0.tar.xz", "多機能エミュレータ", "Multi-system emulator", false },
    { "app-store", "https://github.com/fortheusers/hb-appstore/releases/download/v2.3.2/appstore.nro", "追加ツール1", "Extra tool 1", false },
    { "DBI ru", "https://github.com/rashevskyv/dbi/releases/download/810/DBI.nro", "ディスクバックアップ統合 (DBI)", "Disk Backup Integration (DBI)", false },
    { "DBI en", "https://github.com/Morce3232/DBIPatcher/releases/download/v810/DBI.810.en.nro", "ディスクバックアップ統合 (DBI)", "Disk Backup Integration (DBI)", false },
    { "NX-BootManager", "https://github.com/KranKRival/BootSoundNX/releases/download/1.1.0/BootSoundNX.zip", "起動音管理", "Boot sound manager", false },
    { "Lockpick_RCM", "https://github.com/Atmosphere-NX/Lockpick/releases/download/v1.4.3/Lockpick_RCM.bin", "鍵管理ツール", "Key management tool", false },
    { "Awoo Installer", "https://github.com/AwooInstaller/AwooInstaller/releases/download/v1.0.0/AwooInstaller.nro", "Awoo インストーラ", "Awoo Installer", false },
    { "sys-clk", "https://github.com/retronx-team/sys-clk/releases/download/2.0.1/sys-clk-2.0.1.zip", "システムクロック管理", "System clock manager", false },
    { "JKSV", "https://github.com/J-D-K/JKSV/releases/download/09%2F13%2F2025/JKSV.nro", "セーブデータエクスポート", "Save data exporter", false },
    { "AtmosphereTools", "https://github.com/AtmosphereTeam/AtmosphereTool/releases/download/v0.1.2/AtmosphereTool.zip", "CFW補助ツール", "CFW helper tools", false },
    { "amii-tool", "https://github.com/Amii-Dev/amii-tool/releases/download/v1.0.0/amii-tool.nro", "amiibo ツール", "amiibo tool", false },
    { "nx-vortex", "https://github.com/nh-server/nx-vortex/releases/download/v1.5.0/nx-vortex.nro", "パッケージ管理", "Package manager", false },  
    { "Fizeau", "https://github.com/nh-server/fizeau/releases/download/v1.0.0/fizeau.nro", "ファイル管理", "color manager", false },
    { "nx-ovlloader+", "https://github.com/ppkantorski/nx-ovlloader/releases/download/v1.1.1/nx-ovlloader+.zip", "オーバーレイローダー", "Overlay loader", false },
    { "nx-ovlloader", "http://example.com/nxovlloader.nro", "オーバーレイローダー", "Overlay loader", false },
    { "nx-hbloader", "http://example.com/hbloader.nro", "ホームブリューランチャー", "Homebrew launcher", false },
    { "usbloader", "http://example.com/usbloader.nro", "USB ファイルアクセス", "USB file access", false },
    { "FTPServer", "http://example.com/ftpserver.nro", "FTP サーバ", "FTP Server", false },
    { "NxThemes", "http://example.com/nxthemes.nro", "テーマエディタ", "Theme editor", false },
    { "XorTweak", "http://example.com/xortweak.nro", "システム調整ツール", "System tweaking tool", false },
    { "Joy-con Tool", "http://example.com/jctool.nro", "コントローラ設定", "Controller settings", false },
    { "nx-capture", "http://example.com/nxcapture.nro", "画面キャプチャ", "Screen capture", false },
    { "USB-Gadget", "http://example.com/usbgadget.nro", "USB ガジェットサポート", "USB gadget support", false },
    { "NX-Screenshot", "http://example.com/nxscreenshot.nro", "スクリーンショットツール", "Screenshot tool", false },
    { "HekateHelper", "http://example.com/hekatehelper.nro", "ブート管理支援", "Boot management helper", false },
    { "nx-rename", "http://example.com/nxrename.nro", "ファイル名変更ツール", "Filename renamer", false },
    { "pkg-updater", "http://example.com/pkgupdater.nro", "パッケージ更新", "Package updater", false },
    { "Emu-Launcher", "http://example.com/emulauncher.nro", "エミュレータランチャー", "Emulator launcher", false },
    { "Homebrew Installer", "http://example.com/hbinstaller.nro", "ホームブリューインストーラ", "Homebrew installer", false },
    { "Switch-Linux", "http://example.com/switchlinux.nro", "Linuxブートツール", "Linux boot tool", false },
    { "nx-logger", "http://example.com/nxlogger.nro", "ログ表示ツール", "Logger utility", false },
    { "sound-player", "http://example.com/soundplayer.nro", "オーディオプレーヤー", "Audio player", false },
    { "video-player", "http://example.com/videoplayer.nro", "ビデオプレーヤー", "Video player", false },
    { "netplay-client", "http://example.com/netplay.nro", "ネット対戦クライアント", "Netplay client", false },
    { "nx-multitool", "http://example.com/nxmultitool.nro", "ユーティリティ集", "Utilities suite", false },
    { "icon-creator", "http://example.com/iconcreator.nro", "アイコン作成ツール", "Icon creator", false },
    { "pkgmgr", "http://example.com/pkgmgr.nro", "パッケージ管理ツール", "Package manager CLI", false },
    { "nx-webview", "http://example.com/nxwebview.nro", "Web ビューウィジェット", "Web view widget", false },
    { "language-tools", "http://example.com/langtools.nro", "翻訳/ローカライズ支援", "Translation/localization helper", false },
    { "wifi-tools", "http://example.com/wifitools.nro", "Wi-Fi診断ツール", "Wi-Fi diagnostics", false },
    { "battery-monitor", "http://example.com/battmon.nro", "バッテリーモニタ", "Battery monitor", false },
    { "nx-gpu-tweak", "http://example.com/gputweak.nro", "GPU設定ツール", "GPU tweak", false },
    { "controller-mapper", "http://example.com/ctrlmap.nro", "コントローラマッパー", "Controller mapper", false },
    { "devkit-shell", "http://example.com/devkitshell.nro", "開発キットシェル", "Devkit shell", false },
    { "text-editor", "http://example.com/texteditor.nro", "テキストエディタ", "Text editor", false },
    { "file-sync", "http://example.com/filesync.nro", "ファイル同期", "File sync", false },
    { "qr-reader", "http://example.com/qrreader.nro", "QRコードリーダー", "QR reader", false },
    { "image-viewer", "http://example.com/imageviewer.nro", "画像ビューア", "Image viewer", false },
    { "pdf-viewer", "http://example.com/pdfviewer.nro", "PDFビューワ", "PDF viewer", false },
    { "homebrew-db", "http://example.com/hbdb.nro", "ホームブリューデータベース", "Homebrew database", false },
    { "backup-tool", "http://example.com/backup.nro", "バックアップツール", "Backup tool", false },
    { "restore-tool", "http://example.com/restore.nro", "リストアツール", "Restore tool", false },
    { "system-info", "http://example.com/sysinfo.nro", "システム情報", "System info", false },
    { "cpu-monitor", "http://example.com/cpumon.nro", "CPUモニタ", "CPU monitor", false },
    { "mem-monitor", "http://example.com/memmon.nro", "メモリモニタ", "Memory monitor", false },
    { "net-monitor", "http://example.com/netmon.nro", "ネットワークモニタ", "Network monitor", false },
    { "bridge-tool", "http://example.com/bridge.nro", "ネットワークブリッジ", "Network bridge", false },
    { "ftp-client", "http://example.com/ftpclient.nro", "FTP クライアント", "FTP client", false },
    { "smb-client", "http://example.com/smbclient.nro", "SMB クライアント", "SMB client", false },
    { "nx-debugger", "http://example.com/nxdebug.nro", "デバッガ", "Debugger", false },
    { "perf-tool", "http://example.com/perf.nro", "パフォーマンスツール", "Performance tool", false },
    { "homebrew-updater", "http://example.com/hbupdater.nro", "ホームブリュー更新", "Homebrew updater", false },
    { "theme-manager", "http://example.com/thememgr.nro", "テーマ管理", "Theme manager", false },
    { "archive-tool", "http://example.com/archive.nro", "アーカイブツール", "Archive tool", false },
    { "rom-manager", "http://example.com/rommgr.nro", "ROM 管理", "ROM manager", false },
    { "cheat-engine", "http://example.com/cheat.nro", "チートツール", "Cheat tool", false },
    { "clockSync", "http://example.com/clocksync.nro", "時刻同期", "Clock sync", false },
    { "net-stream", "http://example.com/netstream.nro", "ストリーミングクライアント", "Streaming client", false },
    { "bluetooth-tool", "http://example.com/bttool.nro", "Bluetooth ツール", "Bluetooth tool", false },
    { "ota-installer", "http://example.com/otainst.nro", "OTA インストーラ", "OTA installer", false },
    { "launcher2", "http://example.com/launcher2.nro", "カスタムランチャー", "Custom launcher", false },
    { "wifi-scanner", "http://example.com/wifiscan.nro", "Wi-Fiスキャナ", "Wi-Fi scanner", false },
    { "license-viewer", "http://example.com/license.nro", "ライセンスビューア", "License viewer", false },
    { "chess", "http://example.com/chess.nro", "チェスゲーム", "Chess game", false },
    { "puzzle-game", "http://example.com/puzzle.nro", "パズルゲーム", "Puzzle game", false },
    { "music-creator", "http://example.com/music.nro", "音楽作成ツール", "Music creator", false },
    { "voip-client", "http://example.com/voip.nro", "VoIP クライアント", "VoIP client", false },
    { "retro-tools", "http://example.com/retrotools.nro", "レトロツールキット", "Retro toolkit", false },
    { "powercfg", "http://example.com/powercfg.nro", "電源管理", "Power config", false },
    { "nx-crypto", "http://example.com/crypto.nro", "暗号ユーティリティ", "Crypto utility", false },
    { "calendar", "http://example.com/calendar.nro", "カレンダー", "Calendar", false },
    { "notes", "http://example.com/notes.nro", "メモアプリ", "Notes app", false },
    { "translator", "http://example.com/translator.nro", "翻訳ツール", "Translator", false },
    { "dbi-tool", "http://example.com/dbi_tool.nro", "DBI ツール - ディスクバックアップ統合", "DBI tool - Disk Backup Integration", false },
};
const int g_candidate_count = sizeof(g_candidates) / sizeof(g_candidates[0]);

void scan_installs(InstallItem *items, int count) {
    struct stat st;
    for (int i = 0; i < count; ++i) {
        char path[256];
        snprintf(path, sizeof(path), "sdmc:/switch/%s.nro", items[i].name);
        if (stat(path, &st) == 0) items[i].installed = true; else items[i].installed = false;
    }
}

// Very small HTTP downloader (supports only http://, no chunked, no https)
// Returns 0 on success, non-zero on error.
// Supports http:// and ftp:// (basic passive FTP RETR). HTTPS is not supported here; recommend linking libcurl for HTTPS.
static int http_download_to_file(const char *url, const char *out_path, int *out_bytes, int *out_total, int progress_row, int progress_cols) {
    const char *scheme_end = strstr(url, "://");
    if (!scheme_end) return -1;
    size_t scheme_len = scheme_end - url;
    if (scheme_len == 4 && strncmp(url, "http", 4) == 0) {
        // HTTP (existing implementation)
        const char *host = url + 7;
        const char *path = strchr(host, '/');
        char hostbuf[256]; char pathbuf[1024];
        if (!path) { strcpy(hostbuf, host); strcpy(pathbuf, "/"); }
        else { size_t hn = path - host; if (hn >= sizeof(hostbuf)) return -3; memcpy(hostbuf, host, hn); hostbuf[hn]=0; strncpy(pathbuf, path, sizeof(pathbuf)-1); pathbuf[sizeof(pathbuf)-1]=0; }

        struct addrinfo hints={0}, *res;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostbuf, "80", &hints, &res) != 0) return -4;
        int s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (s < 0) { freeaddrinfo(res); return -5; }
        if (connect(s, res->ai_addr, res->ai_addrlen) != 0) { close(s); freeaddrinfo(res); return -6; }
        freeaddrinfo(res);

        char req[2048]; snprintf(req, sizeof(req), "GET %s HTTP/1.0\r\nHost: %s\r\nConnection: close\r\n\r\n", pathbuf, hostbuf);
        if (send(s, req, strlen(req), 0) < 0) { close(s); return -7; }

        FILE *f = fopen(out_path, "wb"); if (!f) { close(s); return -8; }

        char buf[4096]; int header_done = 0; int content_len = -1; int total_written = 0;
        int rcv;
        char hdr[8192]; int hi = 0; hdr[0]=0;
        while ((rcv = recv(s, buf, sizeof(buf), 0)) > 0) {
            if (!header_done) {
                int copy = rcv; if (hi + copy >= (int)sizeof(hdr)) copy = (int)sizeof(hdr)-1-hi;
                memcpy(hdr+hi, buf, copy); hi += copy; hdr[hi]=0;
                char *end = strstr(hdr, "\r\n\r\n");
                if (end) {
                    header_done = 1;
                    int status = 0; sscanf(hdr, "HTTP/%*d.%*d %d", &status);
                    if (status < 200 || status >= 300) { fclose(f); close(s); return -9; }
                    char *cl = NULL;
                    for (char *q = hdr; *q; ++q) {
                        int match = 1; const char *needle = "Content-Length:";
                        for (int k = 0; needle[k]; ++k) {
                            if (!q[k]) { match = 0; break; }
                            if (tolower((unsigned char)q[k]) != tolower((unsigned char)needle[k])) { match = 0; break; }
                        }
                        if (match) { cl = q; break; }
                    }
                    if (cl) { sscanf(cl, "Content-Length: %d", &content_len); }
                    int header_len = (int)(end - hdr) + 4;
                    int body_bytes_in_buf = hi - header_len;
                    if (body_bytes_in_buf > 0) {
                        if (fwrite(hdr + header_len, 1, body_bytes_in_buf, f) != (size_t)body_bytes_in_buf) { fclose(f); close(s); return -10; }
                        total_written += body_bytes_in_buf;
                    }
                    continue;
                }
            } else {
                if (fwrite(buf, 1, rcv, f) != (size_t)rcv) { fclose(f); close(s); return -11; }
                total_written += rcv;
            }
        }

        fclose(f); close(s);
        if (out_bytes) *out_bytes = total_written;
        if (out_total) *out_total = content_len;
        return 0;
    } else if (scheme_len == 3 && strncmp(url, "ftp", 3) == 0) {
        // Basic FTP (passive) implementation: connect to control port, send USER/PASS/TYPE I/PASV/RETR
        // Note: this is minimal and lacks robust error handling / authentication. It supports anonymous FTP only.
        const char *host = url + 6; // skip ftp://
        const char *path = strchr(host, '/');
        if (!path) return -20;
        char hostbuf[256]; char filepath[1024];
        size_t hn = path - host; if (hn >= sizeof(hostbuf)) return -21; memcpy(hostbuf, host, hn); hostbuf[hn]=0;
        strncpy(filepath, path+1, sizeof(filepath)-1); filepath[sizeof(filepath)-1]=0; // remove leading '/'

        struct addrinfo hints={0}, *res;
        hints.ai_socktype = SOCK_STREAM;
        if (getaddrinfo(hostbuf, "21", &hints, &res) != 0) return -22;
        int ctrl = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (ctrl < 0) { freeaddrinfo(res); return -23; }
        if (connect(ctrl, res->ai_addr, res->ai_addrlen) != 0) { close(ctrl); freeaddrinfo(res); return -24; }
        freeaddrinfo(res);

        // read banner
        char rbuf[512]; int rlen = recv(ctrl, rbuf, sizeof(rbuf)-1, 0); if (rlen <= 0) { close(ctrl); return -25; }

        // send USER anonymous
        send(ctrl, "USER anonymous\r\n", 15, 0);
        recv(ctrl, rbuf, sizeof(rbuf)-1, 0);
        send(ctrl, "PASS anonymous@\r\n", 21, 0);
        recv(ctrl, rbuf, sizeof(rbuf)-1, 0);
        // set binary
        send(ctrl, "TYPE I\r\n", 8, 0);
        recv(ctrl, rbuf, sizeof(rbuf)-1, 0);
        // enter passive
        send(ctrl, "PASV\r\n", 6, 0);
        rlen = recv(ctrl, rbuf, sizeof(rbuf)-1, 0);
        if (rlen <= 0) { close(ctrl); return -26; }
        rbuf[rlen] = '\0';
        // parse (h1,h2,h3,h4,p1,p2)
        int h1,h2,h3,h4,p1,p2;
        char *p = strchr(rbuf, '(');
        if (!p) { close(ctrl); return -27; }
        if (sscanf(p, "(%d,%d,%d,%d,%d,%d)", &h1,&h2,&h3,&h4,&p1,&p2) != 6) { close(ctrl); return -28; }
        int data_port = p1*256 + p2;
        char data_host[64]; snprintf(data_host, sizeof(data_host), "%d.%d.%d.%d", h1,h2,h3,h4);

        // connect data socket
        char portstr[16]; snprintf(portstr, sizeof(portstr), "%d", data_port);
        if (getaddrinfo(data_host, portstr, &hints, &res) != 0) { close(ctrl); return -29; }
        int data = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
        if (data < 0) { freeaddrinfo(res); close(ctrl); return -30; }
        if (connect(data, res->ai_addr, res->ai_addrlen) != 0) { freeaddrinfo(res); close(ctrl); close(data); return -31; }
        freeaddrinfo(res);

        // send RETR <filepath>
        char cmd[1200]; snprintf(cmd, sizeof(cmd), "RETR %s\r\n", filepath);
        send(ctrl, cmd, strlen(cmd), 0);
        recv(ctrl, rbuf, sizeof(rbuf)-1, 0);

        FILE *f = fopen(out_path, "wb"); if (!f) { close(ctrl); close(data); return -32; }
        int n;
        while ((n = recv(data, rbuf, sizeof(rbuf), 0)) > 0) {
            if (fwrite(rbuf, 1, n, f) != (size_t)n) { fclose(f); close(data); close(ctrl); return -33; }
        }
        fclose(f); close(data);
    recv(ctrl, rbuf, sizeof(rbuf)-1, 0);
    close(ctrl);
    if (out_bytes) *out_bytes = 0;
    if (out_total) *out_total = -1;
    return 0;
    } else if (scheme_len == 5 && strncmp(url, "https", 5) == 0) {
#ifdef USE_LIBCURL
        // Use libcurl to fetch HTTPS (and many other protocols).
        CURL *curl = curl_easy_init();
        if (!curl) return -99;
        FILE *f = fopen(out_path, "wb"); if (!f) { curl_easy_cleanup(curl); return -8; }
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, f);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, NULL);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        CURLcode cres = curl_easy_perform(curl);
        fclose(f);
        curl_easy_cleanup(curl);
        if (cres != CURLE_OK) return -100 - (int)cres;
        if (out_bytes) *out_bytes = 0; if (out_total) *out_total = -1;
        return 0;
#else
        // HTTPS not supported in this build; instruct caller to enable libcurl.
        return -99; // unsupported
#endif
    }
    return -1;
}

int staged_install(const char *name, const char *url, int progress_row, int progress_cols) {
    mkdir("sdmc:/switch/.tmp", 0755);
    char tmp_path[512]; snprintf(tmp_path, sizeof(tmp_path), "sdmc:/switch/.tmp/%s.part", name);
    int res = http_download_to_file(url, tmp_path, NULL, NULL, progress_row, progress_cols);
    if (res != 0) return res;
    struct stat st; if (stat(tmp_path, &st) != 0 || st.st_size == 0) return -20;
    char final_path[512]; snprintf(final_path, sizeof(final_path), "sdmc:/switch/%s.nro", name);
    size_t bak_len = strlen(final_path) + 5;
    char *bak_path = malloc(bak_len);
    if (!bak_path) return -22;
    snprintf(bak_path, bak_len, "%s.bak", final_path);
    if (stat(final_path, &st) == 0) {
        rename(final_path, bak_path);
    }
    int ren = rename(tmp_path, final_path);
    free(bak_path);
    if (ren != 0) return -21;
    return 0;
}

int install_local_nro(const char *src_path, int progress_row, int progress_cols) {
    char *dup = strdup(src_path);
    char *p = strrchr(dup, '/'); if (!p) p = strrchr(dup, '\\');
    char *base = p ? p+1 : dup;
    char dest[512]; snprintf(dest, sizeof(dest), "sdmc:/switch/%s", base);
    free(dup);
    struct stat st;
    if (stat(dest, &st) == 0) {
        char bak[600]; snprintf(bak, sizeof(bak), "%s.bak", dest);
        rename(dest, bak);
    }
    FILE *fs = fopen(src_path, "rb"); if (!fs) return -1;
    FILE *fd = fopen(dest, "wb"); if (!fd) { fclose(fs); return -2; }
    char buf[4096]; size_t r; int total = 0;
    while ((r = fread(buf,1,sizeof(buf),fs)) > 0) {
        if (fwrite(buf,1,r,fd) != r) { fclose(fs); fclose(fd); return -3; }
        total += r;
    }
    fclose(fs); fclose(fd);
    return 0;
}
