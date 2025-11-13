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

#define MAX_NODES_PER_CLIENT 100

/**
 * Blur node structure
 */
struct blur_node {
    uint32_t node_id;
    uint32_t client_id;

    // Dimensions
    int width;
    int height;

    // Parameters
    struct wlblur_blur_params params;

    // Statistics
    uint64_t render_count;
    uint64_t last_render_time_us;

    struct blur_node *next;  // Linked list
};

static struct blur_node *node_list = NULL;
static uint32_t next_node_id = 1;

/**
 * Count nodes owned by a client
 */
static int count_client_nodes(uint32_t client_id) {
    int count = 0;
    for (struct blur_node *n = node_list; n; n = n->next) {
        if (n->client_id == client_id) {
            count++;
        }
    }
    return count;
}

/**
 * Create a new blur node
 */
uint32_t blur_node_create(uint32_t client_id, uint32_t width, uint32_t height,
                          const struct wlblur_blur_params *params) {
    // Check resource limits
    int client_node_count = count_client_nodes(client_id);
    if (client_node_count >= MAX_NODES_PER_CLIENT) {
        fprintf(stderr, "[wlblurd] Client %u exceeds node limit (%d/%d)\n",
                client_id, client_node_count, MAX_NODES_PER_CLIENT);
        return 0;
    }

    struct blur_node *node = calloc(1, sizeof(*node));
    if (!node) {
        fprintf(stderr, "[wlblurd] Failed to allocate blur node\n");
        return 0;
    }

    node->node_id = next_node_id++;
    node->client_id = client_id;
    node->width = width;
    node->height = height;
    node->params = *params;
    node->render_count = 0;
    node->last_render_time_us = 0;

    // Add to head of list
    node->next = node_list;
    node_list = node;

    printf("[wlblurd] Created blur node %u for client %u (%dx%d)\n",
           node->node_id, client_id, width, height);

    return node->node_id;
}

/**
 * Lookup a blur node by ID
 */
struct blur_node* blur_node_lookup(uint32_t node_id) {
    for (struct blur_node *n = node_list; n; n = n->next) {
        if (n->node_id == node_id) {
            return n;
        }
    }
    return NULL;
}

/**
 * Destroy a blur node
 */
void blur_node_destroy(uint32_t node_id) {
    struct blur_node **prev = &node_list;

    for (struct blur_node *n = node_list; n; n = n->next) {
        if (n->node_id == node_id) {
            *prev = n->next;
            printf("[wlblurd] Destroyed blur node %u\n", node_id);
            free(n);
            return;
        }
        prev = &n->next;
    }
}

/**
 * Destroy all nodes owned by a client
 */
void blur_node_destroy_client(uint32_t client_id) {
    struct blur_node **prev = &node_list;
    int count = 0;

    while (*prev) {
        struct blur_node *n = *prev;
        if (n->client_id == client_id) {
            *prev = n->next;
            free(n);
            count++;
        } else {
            prev = &n->next;
        }
    }

    if (count > 0) {
        printf("[wlblurd] Cleaned up %d nodes for client %u\n", count, client_id);
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
