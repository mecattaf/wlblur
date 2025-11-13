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
 * Lookup client connection by FD
 */
struct client_connection* client_lookup(int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == client_fd) {
            return &clients[i];
        }
    }
    return NULL;
}

/**
 * Unregister and cleanup client connection
 */
void client_unregister(int client_fd) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (clients[i].active && clients[i].fd == client_fd) {
            printf("[wlblurd] Client disconnected: fd=%d id=%u\n",
                   client_fd, clients[i].client_id);

            // Cleanup all blur nodes owned by this client
            blur_node_destroy_client(clients[i].client_id);

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
    // Dispatch to protocol handler
    handle_client_request(client_fd);
}
