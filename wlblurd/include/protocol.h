/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * protocol.h - IPC protocol definitions
 */

#ifndef WLBLURD_PROTOCOL_H
#define WLBLURD_PROTOCOL_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <stdbool.h>

/*
 * IPC functions for Unix domain socket communication
 */

/**
 * Receive message with optional file descriptor
 *
 * Uses SCM_RIGHTS ancillary data to receive FD alongside message.
 *
 * @param sockfd Socket file descriptor
 * @param buf Buffer to receive message data
 * @param len Length of buffer
 * @param fd_out Output parameter for received FD (set to -1 if none)
 * @return Number of bytes received, or -1 on error
 */
ssize_t recv_with_fd(int sockfd, void *buf, size_t len, int *fd_out);

/**
 * Send message with optional file descriptor
 *
 * @param sockfd Socket file descriptor
 * @param buf Buffer containing message data
 * @param len Length of message
 * @param fd File descriptor to send (or -1 for none)
 * @return Number of bytes sent, or -1 on error
 */
ssize_t send_with_fd(int sockfd, const void *buf, size_t len, int fd);

/*
 * Client connection management
 */

/**
 * Client connection structure
 */
struct client_connection {
    int fd;                    // Socket file descriptor
    uint32_t client_id;        // Unique client identifier
    bool active;               // Connection is active
};

/**
 * Register a new client connection
 *
 * @param client_fd Client socket file descriptor
 * @return Client ID, or 0 on error
 */
uint32_t client_register(int client_fd);

/**
 * Unregister and cleanup client connection
 *
 * @param client_fd Client socket file descriptor
 */
void client_unregister(int client_fd);

/**
 * Handle incoming client data
 *
 * @param client_fd Client socket file descriptor
 */
void handle_client_data(int client_fd);

/*
 * Event loop
 */

/**
 * Run the main event loop with epoll
 *
 * @param server_fd Server socket file descriptor
 * @return 0 on success, -1 on error
 */
int run_event_loop(int server_fd);

#endif /* WLBLURD_PROTOCOL_H */
