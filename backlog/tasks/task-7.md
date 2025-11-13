---
id: task-7
title: "Implement Unix Socket Server for wlblurd"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["daemon", "ipc", "networking"]
milestone: "m-2"
dependencies: ["task-6"]
---

## Description

Implement the Unix domain socket server that listens for compositor connections. This is the communication backbone of the daemon architecture.

**Features:**
- Socket creation at `/run/user/$UID/wlblur.sock`
- Accept multiple client connections
- FD passing via SCM_RIGHTS
- Event loop with epoll/poll
- Signal handling (SIGTERM, SIGINT)

## Acceptance Criteria

- [x] Socket created at correct XDG_RUNTIME_DIR path
- [x] Socket has correct permissions (0700)
- [x] Accepts multiple simultaneous clients
- [x] FD passing works via SCM_RIGHTS
- [x] Event loop handles concurrent requests
- [x] Graceful shutdown on signals
- [x] Logging all operations
- [x] Test client can connect and exchange messages

## Implementation Plan

### Phase 1: Socket Creation

**File**: `wlblurd/src/main.c`

```c
#include <sys/socket.h>
#include <sys/un.h>
#include <signal.h>
#include <errno.h>

static volatile sig_atomic_t running = 1;

static void signal_handler(int signum) {
    running = 0;
}

int main(int argc, char **argv) {
    // Install signal handlers
    signal(SIGTERM, signal_handler);
    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);  // Ignore broken pipe
    
    // Get socket path
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    if (!runtime_dir) {
        runtime_dir = "/run/user/1000";  // Fallback
    }
    
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path),
             "%s/wlblur.sock", runtime_dir);
    
    // Create socket
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        fprintf(stderr, "Failed to create socket: %s\n",
                strerror(errno));
        return 1;
    }
    
    // Remove old socket if exists
    unlink(socket_path);
    
    // Bind
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to bind socket: %s\n",
                strerror(errno));
        close(server_fd);
        return 1;
    }
    
    // Set permissions (owner only)
    chmod(socket_path, 0700);
    
    // Listen
    if (listen(server_fd, 8) < 0) {
        fprintf(stderr, "Failed to listen: %s\n",
                strerror(errno));
        close(server_fd);
        return 1;
    }
    
    printf("[wlblurd] Listening on %s\n", socket_path);
    
    // Event loop (Phase 2)
    run_event_loop(server_fd);
    
    // Cleanup
    close(server_fd);
    unlink(socket_path);
    
    printf("[wlblurd] Shutdown complete\n");
    return 0;
}
```

### Phase 2: Event Loop with epoll

```c
#include <sys/epoll.h>

struct client_connection {
    int fd;
    uint32_t client_id;
    // Per-client state
};

void run_event_loop(int server_fd) {
    // Create epoll instance
    int epoll_fd = epoll_create1(0);
    
    // Add server socket
    struct epoll_event server_event = {
        .events = EPOLLIN,
        .data.fd = server_fd,
    };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &server_event);
    
    struct epoll_event events[32];
    
    while (running) {
        int nfds = epoll_wait(epoll_fd, events, 32, 1000);
        
        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == server_fd) {
                // New connection
                handle_new_connection(epoll_fd, server_fd);
            } else {
                // Client data
                handle_client_data(events[i].data.fd);
            }
        }
    }
    
    close(epoll_fd);
}

void handle_new_connection(int epoll_fd, int server_fd) {
    int client_fd = accept(server_fd, NULL, NULL);
    if (client_fd < 0) {
        perror("accept");
        return;
    }
    
    printf("[wlblurd] New client connected: fd=%d\n", client_fd);
    
    // Add to epoll
    struct epoll_event client_event = {
        .events = EPOLLIN,
        .data.fd = client_fd,
    };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &client_event);
    
    // Register client (Phase 3)
    client_register(client_fd);
}
```

### Phase 3: FD Passing (SCM_RIGHTS)

**File**: `wlblurd/src/ipc.c`

```c
#include <sys/socket.h>

/**
 * Receive message with file descriptor
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
        return n;
    }
    
    // Extract FD from control message
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS) {
        int *fd_ptr = (int*)CMSG_DATA(cmsg);
        *fd_out = *fd_ptr;
    } else {
        *fd_out = -1;
    }
    
    return n;
}

/**
 * Send message with file descriptor
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
    
    return sendmsg(sockfd, &msg, 0);
}
```

### Phase 4: Test Client

**File**: `examples/ipc-test-client.c`

```c
/**
 * Test client for wlblurd socket
 * 
 * Connects to daemon, sends test message with FD
 */
int main() {
    // Connect to daemon
    int sockfd = socket(AF_UNIX, SOCK_STREAM, 0);
    
    const char *runtime_dir = getenv("XDG_RUNTIME_DIR");
    char socket_path[256];
    snprintf(socket_path, sizeof(socket_path),
             "%s/wlblur.sock", runtime_dir);
    
    struct sockaddr_un addr = {0};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);
    
    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "Failed to connect: %s\n", strerror(errno));
        return 1;
    }
    
    printf("✓ Connected to daemon\n");
    
    // Create test file descriptor
    int test_fd = open("/dev/null", O_RDONLY);
    
    // Send test message with FD
    const char *msg = "TEST_MESSAGE";
    ssize_t sent = send_with_fd(sockfd, msg, strlen(msg), test_fd);
    
    printf("✓ Sent message with FD (sent=%zd)\n", sent);
    
    // Receive response
    char response[256];
    int received_fd;
    ssize_t received = recv_with_fd(sockfd, response, sizeof(response),
                                    &received_fd);
    
    printf("✓ Received response: %.*s (fd=%d)\n",
           (int)received, response, received_fd);
    
    close(test_fd);
    if (received_fd >= 0) close(received_fd);
    close(sockfd);
    
    return 0;
}
```

## Deliverables

1. `wlblurd/src/main.c` - Server main loop
2. `wlblurd/src/ipc.c` - FD passing utilities
3. `examples/ipc-test-client.c` - Test client
4. Commit: "feat(daemon): implement Unix socket server with FD passing"
