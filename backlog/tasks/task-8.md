---
id: task-8
title: "Implement IPC Protocol Handler and Message Processing"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["daemon", "ipc", "protocol"]
milestone: "m-2"
dependencies: ["task-7"]
---

## Description

Implement the complete IPC protocol for blur requests based on the specification in `docs/api/ipc-protocol.md`. This includes:
- Message serialization/deserialization
- Operation handlers (CREATE_NODE, RENDER_BLUR, DESTROY_NODE)
- Request/response flow
- Error handling

## Acceptance Criteria

- [x] Protocol messages serialize/deserialize correctly
- [x] CREATE_NODE allocates blur nodes
- [x] RENDER_BLUR performs blur operation
- [x] DESTROY_NODE frees resources
- [x] Error responses sent for invalid requests
- [x] Binary protocol matches specification
- [x] Test client validates all operations

## Implementation Plan

### Phase 1: Protocol Structures

**File**: `wlblurd/include/protocol.h`

```c
/*
 * IPC protocol definitions
 * See: docs/api/ipc-protocol.md
 */

#ifndef WLBLURD_PROTOCOL_H
#define WLBLURD_PROTOCOL_H

#include <stdint.h>
#include <wlblur/blur_params.h>

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

#endif
```

### Phase 2: Message Handlers

**File**: `wlblurd/src/ipc_protocol.c`

```c
#include "protocol.h"
#include "blur_node.h"

/**
 * Handle CREATE_NODE request
 */
struct wlblur_response handle_create_node(
    struct client_connection *client,
    const struct wlblur_request *req
) {
    struct wlblur_response resp = {0};
    
    // Allocate node
    uint32_t node_id = blur_node_create(client->client_id,
                                        req->width,
                                        req->height,
                                        &req->params);
    
    if (node_id == 0) {
        resp.status = WLBLUR_STATUS_OUT_OF_MEMORY;
        return resp;
    }
    
    resp.status = WLBLUR_STATUS_SUCCESS;
    resp.node_id = node_id;
    
    printf("[wlblurd] Created node %u for client %u\n",
           node_id, client->client_id);
    
    return resp;
}

/**
 * Handle RENDER_BLUR request
 */
struct wlblur_response handle_render_blur(
    struct client_connection *client,
    const struct wlblur_request *req,
    int input_fd,
    int *output_fd
) {
    struct wlblur_response resp = {0};
    *output_fd = -1;
    
    // Lookup node
    struct blur_node *node = blur_node_lookup(req->node_id);
    if (!node || node->client_id != client->client_id) {
        resp.status = WLBLUR_STATUS_INVALID_NODE;
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
    
    // Apply blur
    struct wlblur_dmabuf_attribs output_attribs;
    if (!wlblur_apply_blur(get_blur_context(),
                          &input_attribs,
                          &req->params,
                          &output_attribs)) {
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
    
    return resp;
}

/**
 * Handle DESTROY_NODE request
 */
struct wlblur_response handle_destroy_node(
    struct client_connection *client,
    const struct wlblur_request *req
) {
    struct wlblur_response resp = {0};
    
    // Lookup node
    struct blur_node *node = blur_node_lookup(req->node_id);
    if (!node || node->client_id != client->client_id) {
        resp.status = WLBLUR_STATUS_INVALID_NODE;
        return resp;
    }
    
    // Destroy node
    blur_node_destroy(req->node_id);
    
    resp.status = WLBLUR_STATUS_SUCCESS;
    
    printf("[wlblurd] Destroyed node %u\n", req->node_id);
    
    return resp;
}
```

### Phase 3: Request Dispatcher

```c
/**
 * Process incoming request
 */
void handle_client_request(int client_fd) {
    // Receive request + FD
    struct wlblur_request req;
    int input_fd = -1;
    
    ssize_t n = recv_with_fd(client_fd, &req, sizeof(req), &input_fd);
    if (n != sizeof(req)) {
        fprintf(stderr, "[wlblurd] Invalid request size: %zd\n", n);
        return;
    }
    
    // Validate protocol version
    if (req.protocol_version != WLBLUR_PROTOCOL_VERSION) {
        fprintf(stderr, "[wlblurd] Unsupported protocol version: %u\n",
                req.protocol_version);
        return;
    }
    
    // Get client
    struct client_connection *client = client_lookup(client_fd);
    
    // Dispatch
    struct wlblur_response resp;
    int output_fd = -1;
    
    switch (req.op) {
    case WLBLUR_OP_CREATE_NODE:
        resp = handle_create_node(client, &req);
        break;
        
    case WLBLUR_OP_RENDER_BLUR:
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
    send_with_fd(client_fd, &resp, sizeof(resp), output_fd);
    
    // Cleanup
    if (input_fd >= 0) close(input_fd);
    if (output_fd >= 0) close(output_fd);
}
```

## Deliverables

1. `wlblurd/include/protocol.h` - Protocol definitions
2. `wlblurd/src/ipc_protocol.c` - Message handlers
3. Test client validating all operations
4. Commit: "feat(daemon): implement IPC protocol handlers"
