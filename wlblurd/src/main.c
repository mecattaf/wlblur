/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * main.c - Daemon entry point and event loop
 */

#include "protocol.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static volatile sig_atomic_t running = 1;

/**
 * Signal handler for graceful shutdown
 */
static void signal_handler(int signum) {
    (void)signum;
    running = 0;
}

/**
 * Handle new incoming connection
 */
static void handle_new_connection(int epoll_fd, int server_fd) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("[wlblurd] accept");
        return;
    }

    printf("[wlblurd] New client connected: fd=%d\n", client_fd);

    // Add to epoll
    struct epoll_event client_event = {
        .events = EPOLLIN,
        .data.fd = client_fd,
    };

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event) < 0) {
        perror("[wlblurd] epoll_ctl");
        close(client_fd);
        return;
    }

    // Register client
    if (client_register(client_fd) == 0) {
        epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
    }
}

/**
 * Run the main event loop with epoll
 */
int run_event_loop(int server_fd) {
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("[wlblurd] epoll_create1");
        return -1;
    }

    // Add server socket
    struct epoll_event server_event = {
        .events = EPOLLIN,
        .data.fd = server_fd,
    };

    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event) < 0) {
        perror("[wlblurd] epoll_ctl");
        close(epoll_fd);
        return -1;
    }

    struct epoll_event events[32];

    printf("[wlblurd] Event loop started\n");

    while (running) {
        int nfds = epoll_wait(epoll_fd, events, 32, 1000);

        if (nfds < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("[wlblurd] epoll_wait");
            break;
        }

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // New connection
                handle_new_connection(epoll_fd, server_fd);
            } else {
                // Client data
                int client_fd = events[i].data.fd;

                if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                    // Client disconnected or error
                    printf("[wlblurd] Client fd=%d disconnected (epoll event)\n",
                           client_fd);
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
                    client_unregister(client_fd);
                } else if (events[i].events & EPOLLIN) {
                    // Client has data
                    handle_client_data(client_fd);
                }
            }
        }
    }

    close(epoll_fd);
    printf("[wlblurd] Event loop stopped\n");
    return 0;
}

/**
 * Main entry point
 */
int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    printf("[wlblurd] wlblur daemon starting...\n");

    // Install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe

    // Get socket path
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        // Fallback to /tmp if XDG_RUNTIME_DIR is not set
        runtime_dir = "/tmp";
        fprintf(stderr, "[wlblurd] Warning: XDG_RUNTIME_DIR not set, using %s\n",
                runtime_dir);
    }

    char socket_path[256];
    int ret = snprintf(socket_path, sizeof(socket_path),
                      "%s/wlblur.sock", runtime_dir);
    if (ret < 0 || ret >= (int)sizeof(socket_path)) {
        fprintf(stderr, "[wlblurd] Socket path too long\n");
        return 1;
    }

    // Create socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "[wlblurd] Failed to create socket: %s\n",
                strerror(errno));
        return 1;
    }

    // Remove old socket if exists
    unlink(socket_path);

    // Bind
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "[wlblurd] Failed to bind socket: %s\n",
                strerror(errno));
        close(server_fd);
        return 1;
    }

    // Set permissions (owner only)
    if (chmod(socket_path, 0700) < 0) {
        fprintf(stderr, "[wlblurd] Failed to set socket permissions: %s\n",
                strerror(errno));
        close(server_fd);
        unlink(socket_path);
        return 1;
    }

    // Listen
    if (listen(server_fd, 8) < 0) {
        fprintf(stderr, "[wlblurd] Failed to listen: %s\n",
                strerror(errno));
        close(server_fd);
        unlink(socket_path);
        return 1;
    }

    printf("[wlblurd] Listening on %s\n", socket_path);

    // Run event loop
    run_event_loop(server_fd);

    // Cleanup
    close(server_fd);
    unlink(socket_path);

    printf("[wlblurd] Shutdown complete\n");
    return 0;
}
