/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * blur_node.c - Blur node registry
 */

#include "protocol.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define MAX_BLUR_NODES 1024

/**
 * Blur node structure
 */
struct blur_node {
    uint32_t node_id;
    uint32_t client_id;
    uint32_t width;
    uint32_t height;
    struct wlblur_blur_params params;
    bool active;
};

static struct blur_node nodes[MAX_BLUR_NODES];
static uint32_t next_node_id = 1;

/**
 * Create a new blur node
 */
uint32_t blur_node_create(uint32_t client_id, uint32_t width, uint32_t height,
                          const struct wlblur_blur_params *params) {
    // Find free slot
    for (int i = 0; i < MAX_BLUR_NODES; i++) {
        if (!nodes[i].active) {
            nodes[i].node_id = next_node_id++;
            nodes[i].client_id = client_id;
            nodes[i].width = width;
            nodes[i].height = height;
            nodes[i].params = *params;
            nodes[i].active = true;

            printf("[wlblurd] Created blur node %u for client %u (%ux%u)\n",
                   nodes[i].node_id, client_id, width, height);

            return nodes[i].node_id;
        }
    }

    fprintf(stderr, "[wlblurd] Failed to create blur node: no free slots\n");
    return 0;
}

/**
 * Lookup a blur node by ID
 */
struct blur_node* blur_node_lookup(uint32_t node_id) {
    for (int i = 0; i < MAX_BLUR_NODES; i++) {
        if (nodes[i].active && nodes[i].node_id == node_id) {
            return &nodes[i];
        }
    }
    return NULL;
}

/**
 * Destroy a blur node
 */
void blur_node_destroy(uint32_t node_id) {
    for (int i = 0; i < MAX_BLUR_NODES; i++) {
        if (nodes[i].active && nodes[i].node_id == node_id) {
            printf("[wlblurd] Destroyed blur node %u\n", node_id);
            nodes[i].active = false;
            nodes[i].node_id = 0;
            nodes[i].client_id = 0;
            return;
        }
    }
}

/**
 * Destroy all nodes owned by a client
 */
void blur_node_destroy_client(uint32_t client_id) {
    int count = 0;
    for (int i = 0; i < MAX_BLUR_NODES; i++) {
        if (nodes[i].active && nodes[i].client_id == client_id) {
            nodes[i].active = false;
            nodes[i].node_id = 0;
            nodes[i].client_id = 0;
            count++;
        }
    }
    if (count > 0) {
        printf("[wlblurd] Destroyed %d blur nodes for client %u\n",
               count, client_id);
    }
}

/**
 * Get the client ID that owns a node
 */
uint32_t blur_node_get_client(const struct blur_node *node) {
    if (node) {
        return node->client_id;
    }
    return 0;
}
