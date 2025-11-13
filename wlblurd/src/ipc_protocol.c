/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * ipc_protocol.c - Protocol message handlers
 */

#include "protocol.h"
#include "config.h"
#include <wlblur/wlblur.h>
#include <wlblur/dmabuf.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Global blur context (created on startup)
static struct wlblur_context *g_blur_ctx = NULL;

/**
 * Initialize the IPC protocol handler
 *
 * Creates the global blur context for rendering operations.
 */
bool ipc_protocol_init(void) {
    if (g_blur_ctx) {
        return true;  // Already initialized
    }

    g_blur_ctx = wlblur_context_create();
    if (!g_blur_ctx) {
        fprintf(stderr, "[wlblurd] Failed to create blur context: %s\n",
                wlblur_error_string(wlblur_get_error()));
        return false;
    }

    printf("[wlblurd] Blur context initialized\n");
    return true;
}

/**
 * Cleanup the IPC protocol handler
 */
void ipc_protocol_cleanup(void) {
    if (g_blur_ctx) {
        wlblur_context_destroy(g_blur_ctx);
        g_blur_ctx = NULL;
        printf("[wlblurd] Blur context destroyed\n");
    }
}

/**
 * Handle CREATE_NODE request
 */
static struct wlblur_response handle_create_node(
    struct client_connection *client,
    const struct wlblur_request *req
) {
    struct wlblur_response resp = {0};

    // Copy params to properly aligned local variable (req is packed)
    struct wlblur_blur_params params = req->params;

    // Allocate node
    uint32_t node_id = blur_node_create(client->client_id,
                                        req->width,
                                        req->height,
                                        &params);

    if (node_id == 0) {
        resp.status = WLBLUR_STATUS_OUT_OF_MEMORY;
        return resp;
    }

    resp.status = WLBLUR_STATUS_SUCCESS;
    resp.node_id = node_id;

    return resp;
}

/**
 * Handle RENDER_BLUR request
 */
static struct wlblur_response handle_render_blur(
    struct client_connection *client,
    const struct wlblur_request *req,
    int input_fd,
    int *output_fd
) {
    struct wlblur_response resp = {0};
    *output_fd = -1;

    // Lookup node
    struct blur_node *node = blur_node_lookup(req->node_id);
    if (!node || blur_node_get_client(node) != client->client_id) {
        resp.status = WLBLUR_STATUS_INVALID_NODE;
        return resp;
    }

    // Check if blur context is available
    if (!g_blur_ctx) {
        fprintf(stderr, "[wlblurd] Blur context not initialized\n");
        resp.status = WLBLUR_STATUS_RENDER_FAILED;
        return resp;
    }

    // Import input DMA-BUF
    struct wlblur_dmabuf_attribs input_attribs = {
        .width = req->width,
        .height = req->height,
        .format = req->format,
        .modifier = req->modifier,
        .num_planes = 1,
        .planes = {
            {
                .fd = input_fd,
                .stride = req->stride,
                .offset = req->offset,
            }
        },
    };

    // Resolve blur parameters using preset system
    const struct wlblur_blur_params *params;
    struct daemon_config *config = get_global_config();

    if (req->use_preset && req->preset_name[0] != '\0') {
        // Use preset from config
        params = resolve_preset(config, req->preset_name, NULL);
        printf("[wlblurd] Using preset '%s' for node %u\n",
               req->preset_name, req->node_id);
    } else {
        // Use compositor-provided parameters
        // Copy params to properly aligned local variable (req is packed)
        static struct wlblur_blur_params direct_params;
        direct_params = req->params;
        params = &direct_params;
        printf("[wlblurd] Using direct parameters for node %u\n",
               req->node_id);
    }

    // Apply blur
    struct wlblur_dmabuf_attribs output_attribs;
    if (!wlblur_apply_blur(g_blur_ctx,
                          &input_attribs,
                          params,
                          &output_attribs)) {
        fprintf(stderr, "[wlblurd] Blur rendering failed: %s\n",
                wlblur_error_string(wlblur_get_error()));
        resp.status = WLBLUR_STATUS_RENDER_FAILED;
        return resp;
    }

    // Fill response
    resp.status = WLBLUR_STATUS_SUCCESS;
    resp.width = output_attribs.width;
    resp.height = output_attribs.height;
    resp.format = output_attribs.format;
    resp.modifier = output_attribs.modifier;
    resp.stride = output_attribs.planes[0].stride;
    resp.offset = output_attribs.planes[0].offset;

    *output_fd = output_attribs.planes[0].fd;

    printf("[wlblurd] Rendered blur for node %u (%ux%u)\n",
           req->node_id, req->width, req->height);

    return resp;
}

/**
 * Handle DESTROY_NODE request
 */
static struct wlblur_response handle_destroy_node(
    struct client_connection *client,
    const struct wlblur_request *req
) {
    struct wlblur_response resp = {0};

    // Lookup node
    struct blur_node *node = blur_node_lookup(req->node_id);
    if (!node || blur_node_get_client(node) != client->client_id) {
        resp.status = WLBLUR_STATUS_INVALID_NODE;
        return resp;
    }

    // Destroy node
    blur_node_destroy(req->node_id);

    resp.status = WLBLUR_STATUS_SUCCESS;

    return resp;
}

/**
 * Process incoming request
 */
void handle_client_request(int client_fd) {
    // Receive request + FD
    struct wlblur_request req;
    int input_fd = -1;

    ssize_t n = recv_with_fd(client_fd, &req, sizeof(req), &input_fd);
    if (n != sizeof(req)) {
        if (n < 0) {
            perror("[wlblurd] recv_with_fd");
        } else {
            fprintf(stderr, "[wlblurd] Invalid request size: %zd (expected %zu)\n",
                    n, sizeof(req));
        }
        if (input_fd >= 0) {
            close(input_fd);
        }
        return;
    }

    // Validate protocol version
    if (req.protocol_version != WLBLUR_PROTOCOL_VERSION) {
        fprintf(stderr, "[wlblurd] Unsupported protocol version: %u\n",
                req.protocol_version);
        if (input_fd >= 0) {
            close(input_fd);
        }
        return;
    }

    // Get client
    struct client_connection *client = client_lookup(client_fd);
    if (!client) {
        fprintf(stderr, "[wlblurd] Client not found for fd=%d\n", client_fd);
        if (input_fd >= 0) {
            close(input_fd);
        }
        return;
    }

    // Dispatch
    struct wlblur_response resp;
    int output_fd = -1;

    switch (req.op) {
    case WLBLUR_OP_CREATE_NODE:
        resp = handle_create_node(client, &req);
        break;

    case WLBLUR_OP_RENDER_BLUR:
        if (input_fd < 0) {
            fprintf(stderr, "[wlblurd] RENDER_BLUR requires input FD\n");
            resp.status = WLBLUR_STATUS_INVALID_PARAMS;
            break;
        }
        resp = handle_render_blur(client, &req, input_fd, &output_fd);
        break;

    case WLBLUR_OP_DESTROY_NODE:
        resp = handle_destroy_node(client, &req);
        break;

    default:
        fprintf(stderr, "[wlblurd] Unknown operation: %u\n", req.op);
        resp.status = WLBLUR_STATUS_INVALID_PARAMS;
        break;
    }

    // Send response
    ssize_t sent = send_with_fd(client_fd, &resp, sizeof(resp), output_fd);
    if (sent < 0) {
        perror("[wlblurd] send_with_fd");
    }

    // Cleanup
    if (input_fd >= 0) {
        close(input_fd);
    }
    if (output_fd >= 0) {
        close(output_fd);
    }
}
