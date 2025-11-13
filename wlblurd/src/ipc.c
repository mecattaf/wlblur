/*
 * wlblur - Compositor-agnostic blur for Wayland
 * Copyright (C) 2025 mecattaf
 * SPDX-License-Identifier: MIT
 *
 * ipc.c - IPC protocol handler with FD passing
 */

#include "protocol.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

/**
 * Receive message with optional file descriptor
 *
 * Uses SCM_RIGHTS ancillary data to receive FD alongside message.
 */
ssize_t recv_with_fd(int sockfd, void *buf, size_t len, int *fd_out) {
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
        perror("recvmsg");
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
ssize_t send_with_fd(int sockfd, const void *buf, size_t len, int fd) {
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

    ssize_t n = sendmsg(sockfd, &msg, 0);
    if (n < 0) {
        perror("sendmsg");
    }

    return n;
}
