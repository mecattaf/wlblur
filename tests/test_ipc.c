/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * test_ipc.c - IPC protocol tests
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <errno.h>
#include <stdint.h>
#include <stdbool.h>
#include <wlblur/blur_params.h>

// Import protocol definitions from wlblurd
#define WLBLUR_PROTOCOL_VERSION 1

enum wlblur_op {
    WLBLUR_OP_CREATE_NODE = 1,
    WLBLUR_OP_DESTROY_NODE = 2,
    WLBLUR_OP_RENDER_BLUR = 3,
};

enum wlblur_status {
    WLBLUR_STATUS_SUCCESS = 0,
    WLBLUR_STATUS_INVALID_NODE = 1,
    WLBLUR_STATUS_INVALID_PARAMS = 2,
    WLBLUR_STATUS_DMABUF_IMPORT_FAILED = 3,
    WLBLUR_STATUS_DMABUF_EXPORT_FAILED = 4,
    WLBLUR_STATUS_RENDER_FAILED = 5,
    WLBLUR_STATUS_OUT_OF_MEMORY = 6,
};

struct wlblur_request {
    uint32_t protocol_version;
    uint32_t op;
    uint32_t node_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;
    struct wlblur_blur_params params;
} __attribute__((packed));

struct wlblur_response {
    uint32_t status;
    uint32_t node_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;
} __attribute__((packed));

/**
 * Connect to daemon
 */
static int connect_to_daemon(void) {
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp";
    }

    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "%s/wlblur.sock", runtime_dir);

    int sock = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(sock);
        return -1;
    }

    printf("[test] Connected to daemon at %s\n", socket_path);
    return sock;
}

/**
 * Send request and receive response
 */
static bool send_request(int sock, const struct wlblur_request *req,
                         struct wlblur_response *resp) {
    // Send request
    ssize_t sent = send(sock, req, sizeof(*req), 0);
    if (sent != sizeof(*req)) {
        perror("send");
        return false;
    }

    // Receive response
    ssize_t received = recv(sock, resp, sizeof(*resp), 0);
    if (received != sizeof(*resp)) {
        perror("recv");
        return false;
    }

    return true;
}

/**
 * Test CREATE_NODE operation
 */
static bool test_create_node(int sock, uint32_t *node_id_out) {
    printf("[test] Testing CREATE_NODE...\n");

    struct wlblur_request req = {0};
    req.protocol_version = WLBLUR_PROTOCOL_VERSION;
    req.op = WLBLUR_OP_CREATE_NODE;
    req.width = 1920;
    req.height = 1080;
    req.params = wlblur_params_default();

    struct wlblur_response resp = {0};
    if (!send_request(sock, &req, &resp)) {
        fprintf(stderr, "[test] Failed to send CREATE_NODE request\n");
        return false;
    }

    if (resp.status != WLBLUR_STATUS_SUCCESS) {
        fprintf(stderr, "[test] CREATE_NODE failed with status %u\n", resp.status);
        return false;
    }

    if (resp.node_id == 0) {
        fprintf(stderr, "[test] CREATE_NODE returned invalid node_id\n");
        return false;
    }

    *node_id_out = resp.node_id;
    printf("[test] ✓ CREATE_NODE succeeded (node_id=%u)\n", resp.node_id);
    return true;
}

/**
 * Test DESTROY_NODE operation
 */
static bool test_destroy_node(int sock, uint32_t node_id) {
    printf("[test] Testing DESTROY_NODE...\n");

    struct wlblur_request req = {0};
    req.protocol_version = WLBLUR_PROTOCOL_VERSION;
    req.op = WLBLUR_OP_DESTROY_NODE;
    req.node_id = node_id;

    struct wlblur_response resp = {0};
    if (!send_request(sock, &req, &resp)) {
        fprintf(stderr, "[test] Failed to send DESTROY_NODE request\n");
        return false;
    }

    if (resp.status != WLBLUR_STATUS_SUCCESS) {
        fprintf(stderr, "[test] DESTROY_NODE failed with status %u\n", resp.status);
        return false;
    }

    printf("[test] ✓ DESTROY_NODE succeeded\n");
    return true;
}

/**
 * Test invalid node ID handling
 */
static bool test_invalid_node(int sock) {
    printf("[test] Testing invalid node ID handling...\n");

    struct wlblur_request req = {0};
    req.protocol_version = WLBLUR_PROTOCOL_VERSION;
    req.op = WLBLUR_OP_DESTROY_NODE;
    req.node_id = 99999;  // Invalid node ID

    struct wlblur_response resp = {0};
    if (!send_request(sock, &req, &resp)) {
        fprintf(stderr, "[test] Failed to send request\n");
        return false;
    }

    if (resp.status != WLBLUR_STATUS_INVALID_NODE) {
        fprintf(stderr, "[test] Expected INVALID_NODE, got status %u\n", resp.status);
        return false;
    }

    printf("[test] ✓ Invalid node ID correctly rejected\n");
    return true;
}

/**
 * Main test suite
 */
int main(void) {
    printf("\n=== wlblur IPC Protocol Test Suite ===\n\n");

    // Connect to daemon
    int sock = connect_to_daemon();
    if (sock < 0) {
        fprintf(stderr, "[test] ✗ Failed to connect to daemon\n");
        fprintf(stderr, "[test] Make sure wlblurd is running!\n");
        return 1;
    }

    bool all_passed = true;
    uint32_t node_id = 0;

    // Test CREATE_NODE
    if (!test_create_node(sock, &node_id)) {
        fprintf(stderr, "[test] ✗ CREATE_NODE test failed\n");
        all_passed = false;
        goto cleanup;
    }

    // Test invalid node ID
    if (!test_invalid_node(sock)) {
        fprintf(stderr, "[test] ✗ Invalid node test failed\n");
        all_passed = false;
        goto cleanup;
    }

    // Test DESTROY_NODE
    if (!test_destroy_node(sock, node_id)) {
        fprintf(stderr, "[test] ✗ DESTROY_NODE test failed\n");
        all_passed = false;
        goto cleanup;
    }

    // Test destroying already destroyed node
    struct wlblur_request req = {0};
    req.protocol_version = WLBLUR_PROTOCOL_VERSION;
    req.op = WLBLUR_OP_DESTROY_NODE;
    req.node_id = node_id;

    struct wlblur_response resp = {0};
    if (send_request(sock, &req, &resp)) {
        if (resp.status == WLBLUR_STATUS_INVALID_NODE) {
            printf("[test] ✓ Double destroy correctly rejected\n");
        }
    }

cleanup:
    close(sock);

    printf("\n=== Test Results ===\n");
    if (all_passed) {
        printf("✓ All tests passed!\n\n");
        return 0;
    } else {
        printf("✗ Some tests failed\n\n");
        return 1;
    }
}
