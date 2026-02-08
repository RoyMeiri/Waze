#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "server.h"
#include "routing.h"

#ifdef _WIN32
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #pragma comment(lib, "Ws2_32.lib")
#else
  #include <unistd.h>
  #include <arpa/inet.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
#endif

/* ---------- helpers ---------- */

static void trim_crlf(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r')) {
        s[n-1] = '\0';
        n--;
    }
}

/* Reads one line ending with '\n' into buf (null-terminated).
   Returns length, 0 if connection closed, -1 on error. */
static int recv_line(int client_fd, char* buf, int cap) {
    int pos = 0;
    while (pos < cap - 1) {
        char c;
#ifdef _WIN32
        int r = recv((SOCKET)client_fd, &c, 1, 0);
#else
        int r = (int)recv(client_fd, &c, 1, 0);
#endif
        if (r == 0) { /* peer closed */
            if (pos == 0) return 0;
            break;
        }
        if (r < 0) return -1;

        buf[pos++] = c;
        if (c == '\n') break;
    }
    buf[pos] = '\0';
    return pos;
}

static int send_all(int client_fd, const char* s) {
    int len = (int)strlen(s);
    int sent = 0;
    while (sent < len) {
#ifdef _WIN32
        int r = send((SOCKET)client_fd, s + sent, len - sent, 0);
#else
        int r = (int)send(client_fd, s + sent, len - sent, 0);
#endif
        if (r <= 0) return -1;
        sent += r;
    }
    return 0;
}

/* ---------- protocol handlers ---------- */

/* TODO: later connect real routing (Dijkstra/A*) and return edges list + eta */
static void handle_req(Graph* g, int client_fd, int src, int dst) {
    if (src < 0 || src >= g->num_nodes || dst < 0 || dst >= g->num_nodes) {
        send_all(client_fd, "ERR BAD_NODES\n");
        return;
    }

    int max_edges = g->num_nodes > 0 ? g->num_nodes : 1; /* path edges <= num_nodes-1 */
    int* path_edges = (int*)malloc(sizeof(int) * max_edges);
    if (!path_edges) {
        send_all(client_fd, "ERR NO_MEM\n");
        return;
    }

    double cost = 0.0;
    int edge_count = 0;
    int rc = find_route_a_star_path(g, src, dst, &cost, path_edges, max_edges, &edge_count);

    if (rc == 1) {
        send_all(client_fd, "ERR NO_ROUTE\n");
        free(path_edges);
        return;
    }
    if (rc != 0) {
        send_all(client_fd, "ERR ROUTE_FAIL\n");
        free(path_edges);
        return;
    }

    size_t buf_sz = 64 + (size_t)edge_count * 16;
    char* resp = (char*)malloc(buf_sz);
    if (!resp) {
        send_all(client_fd, "ERR NO_MEM\n");
        free(path_edges);
        return;
    }

    int n = snprintf(resp, buf_sz, "ROUTE %.3f %d", cost, edge_count);
    for (int i = 0; i < edge_count && n > 0 && (size_t)n < buf_sz; i++) {
        n += snprintf(resp + n, buf_sz - (size_t)n, " %d", path_edges[i]);
    }
    if (n > 0 && (size_t)n < buf_sz) {
        snprintf(resp + n, buf_sz - (size_t)n, "\n");
    } else {
        /* truncated; indicate failure */
        send_all(client_fd, "ERR ROUTE_FAIL\n");
        free(path_edges);
        free(resp);
        return;
    }

    send_all(client_fd, resp);
    free(path_edges);
    free(resp);
}

static void handle_upd(Graph* g, int client_fd, int edge_id, double speed) {
    if (edge_id < 0 || edge_id >= g->num_edges) {
        send_all(client_fd, "ERR BAD_EDGE\n");
        return;
    }
    if (speed <= 0.0) {
        send_all(client_fd, "ERR BAD_SPEED\n");
        return;
    }

    Edge* e = &g->edges[edge_id];
    const double alpha = (e->observation_count == 0) ? 1.0 : 0.2;
    double measured = e->base_length / speed;

    e->ema_travel_time = alpha * measured + (1.0 - alpha) * e->ema_travel_time;
    e->current_travel_time = e->ema_travel_time;
    e->observation_count++;

    send_all(client_fd, "ACK\n");
}

/* parse one line and respond */
static void handle_line(Graph* g, int client_fd, char* line) {
    trim_crlf(line);
    if (line[0] == '\0') {
        send_all(client_fd, "ERR EMPTY\n");
        return;
    }

    /* REQ <src> <dst> */
    int src, dst;
    if (sscanf(line, "REQ %d %d", &src, &dst) == 2) {
        handle_req(g, client_fd, src, dst);
        return;
    }

    /* UPD <edge_id> <speed> */
    int edge_id;
    double speed;
    if (sscanf(line, "UPD %d %lf", &edge_id, &speed) == 2) {
        handle_upd(g, client_fd, edge_id, speed);
        return;
    }

    send_all(client_fd, "ERR UNKNOWN_CMD\n");
}

/* ---------- server ---------- */

int server_run(Graph* g, int port) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    int listen_fd;

#ifdef _WIN32
    listen_fd = (int)socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if ((SOCKET)listen_fd == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed\n");
        WSACleanup();
        return 2;
    }
#else
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) {
        perror("socket");
        return 2;
    }
#endif

    /* reuse addr (best-effort on windows) */
    {
        int opt = 1;
#ifdef _WIN32
        setsockopt((SOCKET)listen_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));
#else
        setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((unsigned short)port);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);

#ifdef _WIN32
    if (bind((SOCKET)listen_fd, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed\n");
        closesocket((SOCKET)listen_fd);
        WSACleanup();
        return 3;
    }
    if (listen((SOCKET)listen_fd, 64) == SOCKET_ERROR) {
        fprintf(stderr, "listen() failed\n");
        closesocket((SOCKET)listen_fd);
        WSACleanup();
        return 4;
    }
#else
    if (bind(listen_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(listen_fd);
        return 3;
    }
    if (listen(listen_fd, 64) < 0) {
        perror("listen");
        close(listen_fd);
        return 4;
    }
#endif

    fprintf(stderr, "Server listening on port %d...\n", port);

    /* Sequential event loop: accept -> read lines -> respond */
    while (1) {
        struct sockaddr_in client_addr;
#ifdef _WIN32
        int client_len = sizeof(client_addr);
        SOCKET client = accept((SOCKET)listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client == INVALID_SOCKET) {
            fprintf(stderr, "accept() failed\n");
            continue;
        }
        int client_fd = (int)client;
#else
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(listen_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
#endif

        fprintf(stderr, "Client connected.\n");

        char line[1024];
        while (1) {
            int r = recv_line(client_fd, line, (int)sizeof(line));
            if (r == 0) break;     /* closed */
            if (r < 0) {
                fprintf(stderr, "recv error\n");
                break;
            }
            handle_line(g, client_fd, line);
        }

        fprintf(stderr, "Client disconnected.\n");
#ifdef _WIN32
        closesocket((SOCKET)client_fd);
#else
        close(client_fd);
#endif
    }

    /* unreachable in this version */
#ifdef _WIN32
    closesocket((SOCKET)listen_fd);
    WSACleanup();
#else
    close(listen_fd);
#endif
    return 0;
}
