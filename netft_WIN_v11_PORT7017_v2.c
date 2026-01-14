```c
/*
  neat_netft_to_udp.c
  - Reads Net F/T (ATI Mini40 NetBox) over UDP:49152 (RDT request/response)
  - Streams formatted ASCII to UDP 127.0.0.1:7017
  - Logs to <cwd><exp_folder>\<ATI_ID>\ati_data.txt
  - Ctrl+C to stop cleanly (SIGINT)

  Build (MSVC):
    cl /O2 /W4 neat_netft_to_udp.c ws2_32.lib

  Notes:
    - exp_settings.txt should contain a relative folder like: \my_experiment\ (or \\my_experiment\\)
    - Output format matches your MATLAB parser:
      ||Fx: ...||Fy: ...||Fz: ...||Tx: ...||Ty: ...||Tz: ...||t: ...
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#pragma comment(lib, "Ws2_32.lib")

/* Net F/T */
#define NETFT_PORT          49152
#define NETFT_CMD_START     2
#define NETFT_NUM_SAMPLES   1

/* UDP stream out */
#define OUT_IP              "127.0.0.1"
#define OUT_PORT            7017

/* Buffers */
#define LINE_BUF_SZ         1024
#define EXPBUF_SZ           512
#define PATHBUF_SZ          (MAX_PATH * 2)

/* ---------- types ---------- */
typedef uint32_t uint32;
typedef int32_t  int32;
typedef uint16_t uint16;
typedef uint8_t  byte;

typedef struct {
    uint32 rdt_sequence;
    uint32 ft_sequence;
    uint32 status;
    int32  FTData[6];
} RESPONSE;

/* ---------- globals ---------- */
static volatile sig_atomic_t g_stop = 0;

/* ---------- helpers ---------- */
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static int ws_init(void) {
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", rc);
        return 0;
    }
    return 1;
}

static void ws_done(void) {
    WSACleanup();
}

static void trim_newlines(char* s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        n--;
    }
}

static void read_line_stdin(const char* prompt, char* out, size_t out_sz) {
    if (!out || out_sz == 0) return;
    out[0] = '\0';
    printf("%s", prompt);
    fflush(stdout);

    if (fgets(out, (int)out_sz, stdin) == NULL) {
        out[0] = '\0';
        return;
    }
    trim_newlines(out);
}

static int read_file_to_string(const char* filename, char* out, size_t out_sz) {
    FILE* f = fopen(filename, "rb");
    if (!f) return 0;

    size_t n = fread(out, 1, out_sz - 1, f);
    fclose(f);
    out[n] = '\0';
    trim_newlines(out);
    return 1;
}

static int ensure_dir(const char* path) {
    if (!path || !path[0]) return 0;
    DWORD attr = GetFileAttributesA(path);
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
        return 1;
    return CreateDirectoryA(path, NULL) ? 1 : 0;
}

static void join_path(char* dst, size_t dst_sz, const char* a, const char* b) {
    if (!dst || dst_sz == 0) return;
    dst[0] = '\0';
    if (!a) a = "";
    if (!b) b = "";
    _snprintf_s(dst, dst_sz, _TRUNCATE, "%s%s", a, b);
}

static int make_log_path(char* out_path, size_t out_sz, char* out_dir, size_t out_dir_sz,
                         const char* exp_folder, const char* ati_id) {
    char cwd[PATHBUF_SZ];
    DWORD n = GetCurrentDirectoryA((DWORD)sizeof(cwd), cwd);
    if (n == 0 || n >= sizeof(cwd)) return 0;

    char base[PATHBUF_SZ];
    join_path(base, sizeof(base), cwd, exp_folder ? exp_folder : "");

    /* Ensure trailing backslash on base */
    size_t bl = strlen(base);
    if (bl && base[bl - 1] != '\\' && base[bl - 1] != '/') {
        strncat_s(base, sizeof(base), "\\", _TRUNCATE);
    }

    char ati_dir[PATHBUF_SZ];
    _snprintf_s(ati_dir, sizeof(ati_dir), _TRUNCATE, "%s%s", base, ati_id ? ati_id : "ATI");

    if (!ensure_dir(ati_dir)) return 0;

    if (out_dir && out_dir_sz) {
        strncpy_s(out_dir, out_dir_sz, ati_dir, _TRUNCATE);
    }

    _snprintf_s(out_path, out_sz, _TRUNCATE, "%s\\ati_data.txt", ati_dir);
    return 1;
}

static double epoch_seconds_now(void) {
#if defined(_WIN32)
    /* timespec_get exists in MSVC; good enough for plotting/logging */
    struct timespec ts;
    timespec_get(&ts, TIME_UTC);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#else
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1e9;
#endif
}

static SOCKET udp_socket_connect(const char* ip, uint16 port) {
    SOCKET s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (s == INVALID_SOCKET) return INVALID_SOCKET;

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (InetPtonA(AF_INET, ip, &addr.sin_addr) != 1) {
        closesocket(s);
        return INVALID_SOCKET;
    }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s);
        return INVALID_SOCKET;
    }
    return s;
}

static SOCKET udp_socket_connect_host(const char* host, uint16 port) {
    /* supports hostname or IP */
    char port_str[16];
    _snprintf_s(port_str, sizeof(port_str), _TRUNCATE, "%u", (unsigned)port);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;        /* Net F/T typically IPv4 */
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(host, port_str, &hints, &res) != 0 || !res) {
        return INVALID_SOCKET;
    }

    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    if (connect(s, res->ai_addr, (int)res->ai_addrlen) == SOCKET_ERROR) {
        closesocket(s);
        freeaddrinfo(res);
        return INVALID_SOCKET;
    }

    freeaddrinfo(res);
    return s;
}

static void build_rdt_request(byte req[8], uint16 command, uint32 num_samples) {
    *(uint16*)&req[0] = htons(0x1234);
    *(uint16*)&req[2] = htons(command);
    *(uint32*)&req[4] = htonl(num_samples);
}

static int recv_exact(SOCKET s, void* buf, int bytes) {
    int got = recv(s, (char*)buf, bytes, 0);
    return (got == bytes) ? 1 : 0;
}

static void parse_rdt_response(const byte raw[36], RESPONSE* out) {
    out->rdt_sequence = ntohl(*(uint32*)&raw[0]);
    out->ft_sequence  = ntohl(*(uint32*)&raw[4]);
    out->status       = ntohl(*(uint32*)&raw[8]);
    for (int i = 0; i < 6; i++) {
        out->FTData[i] = ntohl(*(int32*)&raw[12 + i * 4]);
    }
}

static void format_line(char* out, size_t out_sz, const RESPONSE* r, double t_epoch) {
    /* Net F/T output units: counts -> convert to SI-ish by /1e6 like your original */
    static const char* AXES[6] = {"Fx","Fy","Fz","Tx","Ty","Tz"};

    /* r->FTData already host order */
    double v[6];
    for (int i = 0; i < 6; i++) v[i] = (double)r->FTData[i] / 1000000.0;

    _snprintf_s(out, out_sz, _TRUNCATE,
        "||%s: %.7f||%s: %.7f||%s: %.7f||%s: %.7f||%s: %.7f||%s: %.7f||t: %.9f\n",
        AXES[0], v[0], AXES[1], v[1], AXES[2], v[2],
        AXES[3], v[3], AXES[4], v[4], AXES[5], v[5],
        t_epoch
    );
}

/* ---------- main ---------- */
int main(void) {
    signal(SIGINT, on_sigint);

    if (!ws_init()) return 1;

    char ip_addr[64];
    char ati_id[64];
    char exp_folder[EXPBUF_SZ];

    read_line_stdin("Enter IP Address (or hostname) for the ATI Net F/T:\n> ", ip_addr, sizeof(ip_addr));
    if (ip_addr[0] == '\0') { fprintf(stderr, "No IP/host given.\n"); ws_done(); return 1; }

    read_line_stdin("Enter the ATI ID (folder name):\n> ", ati_id, sizeof(ati_id));
    if (ati_id[0] == '\0') strncpy_s(ati_id, sizeof(ati_id), "ATI", _TRUNCATE);

    if (!read_file_to_string("exp_settings.txt", exp_folder, sizeof(exp_folder))) {
        fprintf(stderr, "Failed to read exp_settings.txt\n");
        ws_done();
        return 1;
    }

    char log_path[PATHBUF_SZ];
    char log_dir[PATHBUF_SZ];
    if (!make_log_path(log_path, sizeof(log_path), log_dir, sizeof(log_dir), exp_folder, ati_id)) {
        fprintf(stderr, "Failed to create log directory.\n");
        ws_done();
        return 1;
    }

    FILE* logf = fopen(log_path, "wb");
    if (!logf) {
        fprintf(stderr, "Failed to open log file: %s\n", log_path);
        ws_done();
        return 1;
    }
    printf("Logging to: %s\n", log_path);

    SOCKET s_netft = udp_socket_connect_host(ip_addr, NETFT_PORT);
    if (s_netft == INVALID_SOCKET) {
        fprintf(stderr, "Failed to connect to Net F/T at %s:%d\n", ip_addr, NETFT_PORT);
        fclose(logf);
        ws_done();
        return 1;
    }

    SOCKET s_out = udp_socket_connect(OUT_IP, OUT_PORT);
    if (s_out == INVALID_SOCKET) {
        fprintf(stderr, "Failed to connect output UDP at %s:%d\n", OUT_IP, OUT_PORT);
        closesocket(s_netft);
        fclose(logf);
        ws_done();
        return 1;
    }

    byte request[8];
    build_rdt_request(request, NETFT_CMD_START, NETFT_NUM_SAMPLES);

    byte raw[36];
    RESPONSE resp;

    char line[LINE_BUF_SZ];

    printf("Streaming... (Ctrl+C to stop)\n");
    while (!g_stop) {
        int sent = send(s_netft, (const char*)request, (int)sizeof(request), 0);
        if (sent != (int)sizeof(request)) break;

        if (!recv_exact(s_netft, raw, (int)sizeof(raw))) break;

        parse_rdt_response(raw, &resp);

        double t_epoch = epoch_seconds_now();
        format_line(line, sizeof(line), &resp, t_epoch);

        /* stdout */
        fputs(line, stdout);

        /* UDP out */
        send(s_out, line, (int)strlen(line), 0);

        /* file log */
        fputs(line, logf);
        fflush(logf);
    }

    printf("Exiting safely.\n");

    closesocket(s_out);
    closesocket(s_netft);
    fclose(logf);
    ws_done();
    return 0;
}
```
