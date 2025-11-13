/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * client.c - Per-client state management
 */

#include "protocol.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

#define MAX_CLIENTS 64

static struct client_connection clients[MAX_CLIENTS];
static uint32_t next_client_id = 1;

/**
 * Register a new client connection
 */
uint32_t client_register(int client_fd) {
    // Find free slot
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (!clients[i].active) {
            clients[i].fd = client_fd;
            clients[i].client_id = next_client_id++;
            clients[i].active = true;

            printf("[wlblurd] Client registered: fd=%d id=%u\n",
                   client_fd, clients[i].client_id);

            return clients[i].client_id;
        }
    }

    fprintf(stderr, "[wlblurd] Failed to register client: no free slots\n");
    return 0;
}

/**
 * Unregister and cleanup client connection
 */
void client_unregister(int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == client_fd) {
            printf("[wlblurd] Client disconnected: fd=%d id=%u\n",
                   client_fd, clients[i].client_id);

            clients[i].active = false;
            clients[i].fd = -1;
            clients[i].client_id = 0;
            close(client_fd);
            return;
        }
    }
}

/**
 * Handle incoming client data
 */
void handle_client_data(int client_fd) {
    char buffer[4096];
    int received_fd = -1;

    ssize_t n = recv_with_fd(client_fd, buffer, sizeof(buffer) - 1, &received_fd);

    if (n <= 0) {
        if (n < 0) {
            perror("[wlblurd] recv_with_fd");
        }
        client_unregister(client_fd);
        return;
    }

    buffer[n] = '\0';

    printf("[wlblurd] Received from client fd=%d: '%.*s'",
           client_fd, (int)n, buffer);
    if (received_fd >= 0) {
        printf(" (with fd=%d)", received_fd);
    }
    printf("\n");

    // Echo response with FD if one was received
    const char *response = "ACK";
    ssize_t sent = send_with_fd(client_fd, response, strlen(response), received_fd);

    if (sent < 0) {
        perror("[wlblurd] send_with_fd");
        client_unregister(client_fd);
        return;
    }

    printf("[wlblurd] Sent response to client fd=%d: '%s'", client_fd, response);
    if (received_fd >= 0) {
        printf(" (echoed fd=%d)", received_fd);
        close(received_fd);  // Close our copy of the FD
    }
    printf("\n");
}
