#include "can_injector.h"
#include "can_frame.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef _WIN32
  #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
  #endif
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef int socklen_t;
#else
  #include <unistd.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <errno.h>
#endif

int can_injector_parse_line(const char *line, can_frame_t *out) {
    return can_frame_from_string(line, out);
}

#ifdef _WIN32
static int winsock_init(void) {
    static int inited = 0;
    if (inited) return 0;
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) return -1;
    inited = 1;
    return 0;
}
#endif

int can_injector_run_server(uint16_t port, can_injector_callback_t cb, void *user) {
    if (!cb) return -1;

#ifdef _WIN32
    if (winsock_init() != 0) {
        fprintf(stderr, "[injector] WSAStartup failed\n");
        return -1;
    }
#endif

    int srv = socket(AF_INET, SOCK_STREAM, 0);
    if (srv < 0) {
        perror("[injector] socket");
        return -1;
    }

    int opt = 1;
#ifdef _WIN32
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
    setsockopt(srv, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(srv, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[injector] bind");
#ifdef _WIN32
        closesocket(srv);
#else
        close(srv);
#endif
        return -1;
    }

    if (listen(srv, 5) < 0) {
        perror("[injector] listen");
#ifdef _WIN32
        closesocket(srv);
#else
        close(srv);
#endif
        return -1;
    }

    printf("[CAN INJECTOR] Listening on TCP port %u\n", (unsigned)port);
    printf("[CAN INJECTOR] Send lines like: \"0x100 1 01\"  (one frame per line)\n");
    printf("[CAN INJECTOR] Press Ctrl-C to stop.\n");

    for (;;) {
        struct sockaddr_in cli;
        socklen_t clilen = sizeof(cli);
        int cfd = accept(srv, (struct sockaddr*)&cli, &clilen);
        if (cfd < 0) {
            perror("[injector] accept");
            continue;
        }

        char cli_ip[64] = {0};
#ifdef _WIN32
        InetNtopA(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
#else
        inet_ntop(AF_INET, &cli.sin_addr, cli_ip, sizeof(cli_ip));
#endif
        printf("[injector] Client connected: %s:%d\n", cli_ip, ntohs(cli.sin_port));

        /* Line-buffered receiver: 1024 byte line max */
        char buf[2048];
        size_t used = 0;
        char recv_tmp[512];

        while (1) {
            int n;
#ifdef _WIN32
            n = recv(cfd, recv_tmp, sizeof(recv_tmp)-1, 0);
#else
            n = recv(cfd, recv_tmp, sizeof(recv_tmp)-1, 0);
#endif
            if (n <= 0) break;
            recv_tmp[n] = '\0';

            /* Append to buf and process lines */
            for (int i = 0; i < n; i++) {
                char c = recv_tmp[i];
                if (c == '\n' || c == '\r') {
                    if (used == 0) continue; /* skip empty */
                    buf[used] = '\0';
                    can_frame_t frame;
                    memset(&frame, 0, sizeof(frame));
                    if (can_injector_parse_line(buf, &frame) == 0) {
                        cb(&frame, user);
                    } else {
                        printf("[injector] parse error: \"%s\"\n", buf);
                    }
                    used = 0;
                } else {
                    if (used + 1 < sizeof(buf)) buf[used++] = c;
                }
            }
        }

#ifdef _WIN32
        closesocket(cfd);
#else
        close(cfd);
#endif
        printf("[injector] Client disconnected\n");
    }

    /* unreachable */
    return 0;
}
