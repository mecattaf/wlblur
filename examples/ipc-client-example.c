/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * ipc-client-example.c - Test client for wlblurd socket
 *
 * Demonstrates:
 * - Connecting to Unix domain socket
 * - Sending messages with file descriptor passing (SCM_RIGHTS)
 * - Receiving responses with file descriptors
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>

/**
 * Receive message with optional file descriptor
 */
static ssize_t recv_with_fd(int sockfd, void *buf, size_t len, int *fd_out) {
    struct msghdr msg = {0};
    struct iovec iov = {
        .iov_base = buf,
        .iov_len = len,
    };

    char control_buf[CMSG_SPACE(sizeof(int))];

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control_buf;
    msg.msg_controllen = sizeof(control_buf);

    ssize_t n = recvmsg(sockfd, &msg, 0);
    if (n < 0) {
        return n;
    }

    // Extract FD from control message
    *fd_out = -1;
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS) {
        int *fd_ptr = (int*)CMSG_DATA(cmsg);
        *fd_out = *fd_ptr;
    }

    return n;
}

/**
 * Send message with optional file descriptor
 */
static ssize_t send_with_fd(int sockfd, const void *buf, size_t len, int fd) {
    struct msghdr msg = {0};
    struct iovec iov = {
        .iov_base = (void*)buf,
        .iov_len = len,
    };

    char control_buf[CMSG_SPACE(sizeof(int))];

    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    if (fd >= 0) {
        msg.msg_control = control_buf;
        msg.msg_controllen = sizeof(control_buf);

        struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int));
        *(int*)CMSG_DATA(cmsg) = fd;
    }

    return sendmsg(sockfd, &msg, 0);
}

/**
 * Main test client
 */
int main(int argc, char **argv) {
    printf("=== wlblurd IPC Test Client ===\n\n");

    // Get socket path
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/tmp";
        printf("Warning: XDG_RUNTIME_DIR not set, using %s\n", runtime_dir);
    }

    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path), "%s/wlblur.sock", runtime_dir);

    printf("[1] Connecting to %s...\n", socket_path);

    // Create socket
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (sockfd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n", strerror(errno));
        return 1;
    }

    // Connect to daemon
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to connect: %s\n", strerror(errno));
        fprintf(stderr, "Make sure wlblurd is running!\n");
        close(sockfd);
        return 1;
    }

    printf("    Connected to daemon\n\n");

    // Test 1: Send message without FD
    printf("[2] Test 1: Sending message without FD...\n");
    const char *msg1 = "HELLO_DAEMON";
    ssize_t sent = send_with_fd(sockfd, msg1, strlen(msg1), -1);
    if (sent < 0) {
        fprintf(stderr, "Failed to send message: %s\n", strerror(errno));
        close(sockfd);
        return 1;
    }
    printf("    Sent: '%s' (%zd bytes)\n", msg1, sent);

    // Receive response
    char response[256];
    int received_fd = -1;
    ssize_t received = recv_with_fd(sockfd, response, sizeof(response) - 1,
                                    &received_fd);
    if (received < 0) {
        fprintf(stderr, "Failed to receive response: %s\n", strerror(errno));
        close(sockfd);
        return 1;
    }

    response[received] = '\0';
    printf("    Received: '%s' (%zd bytes)\n", response, received);
    if (received_fd >= 0) {
        printf("    ERROR: Unexpected FD received: %d\n", received_fd);
        close(received_fd);
    }
    printf("\n");

    // Test 2: Send message with FD
    printf("[3] Test 2: Sending message with FD...\n");

    // Open a test file descriptor
    int test_fd = open("/dev/null", O_RDONLY);
    if (test_fd < 0) {
        fprintf(stderr, "Failed to open /dev/null: %s\n", strerror(errno));
        close(sockfd);
        return 1;
    }
    printf("    Opened /dev/null as fd=%d\n", test_fd);

    const char *msg2 = "TEST_WITH_FD";
    sent = send_with_fd(sockfd, msg2, strlen(msg2), test_fd);
    if (sent < 0) {
        fprintf(stderr, "Failed to send message with FD: %s\n", strerror(errno));
        close(test_fd);
        close(sockfd);
        return 1;
    }
    printf("    Sent: '%s' with fd=%d (%zd bytes)\n", msg2, test_fd, sent);

    // Receive response with echoed FD
    received_fd = -1;
    received = recv_with_fd(sockfd, response, sizeof(response) - 1,
                           &received_fd);
    if (received < 0) {
        fprintf(stderr, "Failed to receive response: %s\n", strerror(errno));
        close(test_fd);
        close(sockfd);
        return 1;
    }

    response[received] = '\0';
    printf("    Received: '%s' (%zd bytes)\n", response, received);
    if (received_fd >= 0) {
        printf("    Received echoed fd=%d\n", received_fd);
        close(received_fd);
    } else {
        printf("    ERROR: No FD received in response!\n");
    }
    printf("\n");

    // Cleanup
    close(test_fd);
    close(sockfd);

    printf("=== All tests completed successfully ===\n");
    return 0;
}
