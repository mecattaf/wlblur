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
#include <wlblur/blur_params.h>

/*
 * IPC Protocol Definitions
 * See: docs/api/ipc-protocol.md
 */

#define WLBLUR_PROTOCOL_VERSION 1

/**
 * Operation codes
 */
enum wlblur_op {
    WLBLUR_OP_CREATE_NODE = 1,
    WLBLUR_OP_DESTROY_NODE = 2,
    WLBLUR_OP_RENDER_BLUR = 3,
};

/**
 * Status codes
 */
enum wlblur_status {
    WLBLUR_STATUS_SUCCESS = 0,
    WLBLUR_STATUS_INVALID_NODE = 1,
    WLBLUR_STATUS_INVALID_PARAMS = 2,
    WLBLUR_STATUS_DMABUF_IMPORT_FAILED = 3,
    WLBLUR_STATUS_DMABUF_EXPORT_FAILED = 4,
    WLBLUR_STATUS_RENDER_FAILED = 5,
    WLBLUR_STATUS_OUT_OF_MEMORY = 6,
};

/**
 * Request message
 *
 * Followed by DMA-BUF FD via SCM_RIGHTS (for RENDER_BLUR)
 */
struct wlblur_request {
    uint32_t protocol_version;
    uint32_t op;
    uint32_t node_id;

    // Texture dimensions
    uint32_t width;
    uint32_t height;

    // DMA-BUF format
    uint32_t format;      // DRM_FORMAT_*
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;

    // Blur parameters
    struct wlblur_blur_params params;
} __attribute__((packed));

/**
 * Response message
 *
 * Followed by result DMA-BUF FD via SCM_RIGHTS (on success)
 */
struct wlblur_response {
    uint32_t status;
    uint32_t node_id;  // For CREATE_NODE

    // Result buffer attributes (for RENDER_BLUR)
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;
} __attribute__((packed));

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
 * Lookup client connection by FD
 *
 * @param client_fd Client socket file descriptor
 * @return Pointer to client connection or NULL if not found
 */
struct client_connection* client_lookup(int client_fd);

/**
 * Handle incoming client data
 *
 * @param client_fd Client socket file descriptor
 */
void handle_client_data(int client_fd);

/*
 * Blur node management
 */

struct blur_node;

/**
 * Create a new blur node
 *
 * @param client_id Client that owns this node
 * @param width Node width
 * @param height Node height
 * @param params Blur parameters
 * @return Node ID or 0 on error
 */
uint32_t blur_node_create(uint32_t client_id, uint32_t width, uint32_t height,
                          const struct wlblur_blur_params *params);

/**
 * Lookup a blur node by ID
 *
 * @param node_id Node ID
 * @return Pointer to node or NULL if not found
 */
struct blur_node* blur_node_lookup(uint32_t node_id);

/**
 * Destroy a blur node
 *
 * @param node_id Node ID to destroy
 */
void blur_node_destroy(uint32_t node_id);

/**
 * Destroy all nodes owned by a client
 *
 * @param client_id Client ID
 */
void blur_node_destroy_client(uint32_t client_id);

/**
 * Get the client ID that owns a node
 *
 * @param node Node pointer
 * @return Client ID
 */
uint32_t blur_node_get_client(const struct blur_node *node);

/*
 * Protocol initialization
 */

/**
 * Initialize the IPC protocol handler
 *
 * @return true on success, false on failure
 */
bool ipc_protocol_init(void);

/**
 * Cleanup the IPC protocol handler
 */
void ipc_protocol_cleanup(void);

/**
 * Process incoming client request
 *
 * @param client_fd Client socket file descriptor
 */
void handle_client_request(int client_fd);

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
