# wlblurd Daemon Architecture

**Document Version:** 1.0
**Last Updated:** 2025-01-15
**Status:** Design Complete, Implementation Pending

---

## Purpose

wlblurd is a multi-compositor blur service daemon that provides centralized blur rendering for all Wayland compositors on a system. It exposes a Unix socket IPC interface, receives backdrop textures via DMA-BUF, and returns blurred results using libwlblur.

**Key Goals:**
- **Multi-Client Support:** Serve multiple compositors simultaneously
- **Resource Isolation:** Per-client state management and quotas
- **Low Latency:** <0.2ms IPC overhead
- **Crash Resilience:** Client disconnects don't affect other clients
- **Simple Deployment:** systemd socket activation, auto-start

---

## Daemon Structure

```
wlblurd/
├── src/
│   ├── main.c               # Entry point, event loop (~150 lines)
│   ├── ipc.c                # IPC server, socket handling (~300 lines)
│   ├── ipc_protocol.c       # Message serialization (~100 lines)
│   ├── client.c             # Per-client state (~150 lines)
│   ├── blur_node.c          # Virtual scene graph (~200 lines)
│   ├── buffer_registry.c    # DMA-BUF tracking (~100 lines)
│   └── config.c             # Configuration loading (~50 lines)
│
├── include/
│   └── protocol.h           # IPC message definitions (~150 lines)
│
└── systemd/
    └── wlblur.service       # systemd user unit

Total: ~1050 lines C code
```

---

## System Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      wlblurd Process                        │
│                                                             │
│  ┌────────────────────────────────────────────────────┐   │
│  │  Main Event Loop (epoll-based)                     │   │
│  │  • Unix socket listener                            │   │
│  │  • Client connections (accept)                     │   │
│  │  • Message reception (recvmsg + SCM_RIGHTS)        │   │
│  │  • Timeout management                              │   │
│  └────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ↓                                  │
│  ┌────────────────────────────────────────────────────┐   │
│  │  IPC Protocol Handler                              │   │
│  │  • Request parsing                                 │   │
│  │  • Validation                                      │   │
│  │  • Response generation                             │   │
│  └────────────────────────────────────────────────────┘   │
│                          │                                  │
│          ┌───────────────┼───────────────┐                 │
│          ↓               ↓               ↓                 │
│  ┌──────────────┐ ┌─────────────┐ ┌──────────────┐       │
│  │ Client       │ │   Blur      │ │   Buffer     │       │
│  │ Registry     │ │   Nodes     │ │   Registry   │       │
│  │              │ │             │ │              │       │
│  │ • client_id  │ │ • blur_id   │ │ • buffer_id  │       │
│  │ • Resources  │ │ • Params    │ │ • DMA-BUF FD │       │
│  │ • Quotas     │ │ • State     │ │ • Ownership  │       │
│  └──────────────┘ └─────────────┘ └──────────────┘       │
│                          │                                  │
│                          ↓                                  │
│  ┌────────────────────────────────────────────────────┐   │
│  │  libwlblur Integration                             │   │
│  │  • Single context (shared across all clients)      │   │
│  │  • DMA-BUF import/export                           │   │
│  │  • Blur rendering                                  │   │
│  └────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
```

---

## IPC Protocol

### Transport Layer

**Socket Type:** Unix domain socket, `SOCK_SEQPACKET`
**Socket Path:** `/run/user/$UID/wlblur.sock`
**FD Passing:** `SCM_RIGHTS` for DMA-BUF file descriptors

**Why SEQPACKET?**
- Message boundaries preserved (no manual framing)
- Reliable, ordered delivery
- Connection-oriented (client tracking)
- FD passing support

### Message Format

#### Request Header

```c
struct blur_request_header {
    uint32_t magic;           // 0x424C5552 ("BLUR")
    uint32_t version;         // Protocol version (1)
    uint32_t client_id;       // Client identifier
    uint32_t sequence;        // Request sequence number
    uint32_t opcode;          // Operation code
    uint32_t payload_size;    // Size of operation-specific payload
} __attribute__((packed));
```

**Field Details:**
- `magic`: Protocol identifier, reject invalid messages
- `version`: Forward compatibility (current: 1)
- `client_id`: Assigned on first connection (0 = handshake)
- `sequence`: For request/response matching
- `opcode`: Operation to perform (see below)
- `payload_size`: Bytes following header

#### Response Header

```c
struct blur_response_header {
    uint32_t magic;           // 0x424C5552
    uint32_t version;         // 1
    uint32_t sequence;        // Matches request sequence
    int32_t status;           // 0 = success, <0 = error code
    uint32_t payload_size;    // Response data size
} __attribute__((packed));
```

### Operation Codes

```c
enum blur_opcode {
    BLUR_OP_HANDSHAKE = 0,        // Initial connection
    BLUR_OP_CREATE_NODE = 1,      // Create blur node
    BLUR_OP_DESTROY_NODE = 2,     // Destroy blur node
    BLUR_OP_IMPORT_DMABUF = 3,    // Import DMA-BUF texture
    BLUR_OP_RELEASE_BUFFER = 4,   // Release buffer
    BLUR_OP_RENDER = 5,            // Perform blur rendering
    BLUR_OP_CONFIGURE = 6,         // Configure node parameters
    BLUR_OP_PING = 7,              // Keep-alive
};
```

---

## Protocol Operations

### 1. Handshake

**Purpose:** Establish client session, assign `client_id`

**Request:**
```c
struct blur_handshake_request {
    struct blur_request_header header;  // client_id = 0, opcode = HANDSHAKE
    char compositor_name[32];           // "scroll", "niri", etc.
    uint32_t compositor_version;
};
```

**Response:**
```c
struct blur_handshake_response {
    struct blur_response_header header;
    uint32_t client_id;                // Assigned client ID
    uint32_t daemon_version;
    uint32_t protocol_version;
};
```

**Flow:**
```
Compositor                           Daemon
    │                                   │
    │  connect(/run/user/1000/wlblur.sock)
    ├──────────────────────────────────►│
    │                                   │
    │  send(BLUR_OP_HANDSHAKE)          │
    ├──────────────────────────────────►│
    │                                   │  Allocate client state
    │                                   │  Assign client_id = 1
    │                                   │
    │  recv(client_id = 1)              │
    │◄──────────────────────────────────┤
    │                                   │
```

### 2. Create Blur Node

**Purpose:** Allocate virtual blur node for a window

**Request:**
```c
struct blur_create_node_request {
    struct blur_request_header header;  // opcode = CREATE_NODE
    int32_t width, height;              // Initial dimensions
};
```

**Response:**
```c
struct blur_create_node_response {
    struct blur_response_header header;
    uint32_t blur_id;                  // Assigned blur node ID
};
```

**Implementation:**
```c
// blur_node.c
struct blur_node {
    uint32_t blur_id;
    uint32_t client_id;
    int32_t width, height;
    struct wlblur_params params;  // Blur parameters
    bool active;
};

uint32_t create_blur_node(uint32_t client_id, int32_t width, int32_t height) {
    struct blur_node* node = malloc(sizeof(*node));
    node->blur_id = next_blur_id++;
    node->client_id = client_id;
    node->width = width;
    node->height = height;
    node->params = default_params;
    node->active = true;

    // Add to registry
    hash_map_insert(blur_nodes, node->blur_id, node);

    return node->blur_id;
}
```

### 3. Import DMA-BUF

**Purpose:** Import compositor texture for blur rendering

**Request:**
```c
struct blur_import_dmabuf_request {
    struct blur_request_header header;  // opcode = IMPORT_DMABUF
    int32_t width, height;
    uint32_t format;                    // DRM fourcc (DRM_FORMAT_ARGB8888)
    uint64_t modifier;
    uint32_t num_planes;
    uint32_t offset[4];
    uint32_t stride[4];
    // DMA-BUF FD sent via SCM_RIGHTS
};
```

**Response:**
```c
struct blur_import_dmabuf_response {
    struct blur_response_header header;
    uint32_t buffer_id;                // Assigned buffer ID
};
```

**FD Passing:**
```c
// Receive DMA-BUF FD
int recv_dmabuf_fd(int sockfd) {
    struct msghdr msg = {0};
    struct iovec iov[1];
    char buf[256];

    iov[0].iov_base = buf;
    iov[0].iov_len = sizeof(buf);
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    // Control message for FD
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    recvmsg(sockfd, &msg, 0);

    // Extract FD
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    int fd = *(int*)CMSG_DATA(cmsg);
    return fd;
}
```

### 4. Render Blur

**Purpose:** Execute blur on imported DMA-BUF, return blurred result

**Request:**
```c
struct blur_render_request {
    struct blur_request_header header;  // opcode = RENDER
    uint32_t blur_id;                   // Blur node
    uint32_t input_buffer_id;           // Input texture
    struct wlblur_params params;        // Blur parameters

    // Damage region (optional optimization)
    uint32_t num_damage_rects;
    struct {
        int32_t x, y;
        int32_t width, height;
    } damage[32];
};
```

**Response:**
```c
struct blur_render_response {
    struct blur_response_header header;
    uint32_t output_buffer_id;         // Blurred texture buffer ID
    // DMA-BUF FD sent via SCM_RIGHTS
};
```

**Implementation:**
```c
// ipc.c
int handle_render_request(struct client_state* client, struct blur_render_request* req) {
    // 1. Validate request
    struct blur_node* node = get_blur_node(req->blur_id, client->client_id);
    if (!node) return -EINVAL;

    struct buffer_entry* input_buf = get_buffer(req->input_buffer_id, client->client_id);
    if (!input_buf) return -EINVAL;

    // 2. Import input DMA-BUF to libwlblur texture
    wlblur_texture_t* input_tex = wlblur_import_dmabuf(ctx, &input_buf->dmabuf);

    // 3. Render blur
    wlblur_texture_t* output_tex = wlblur_render(ctx, input_tex, &req->params);

    // 4. Export to DMA-BUF
    struct wlblur_dmabuf output_dmabuf;
    wlblur_export_dmabuf(output_tex, &output_dmabuf);

    // 5. Register output buffer
    uint32_t output_buffer_id = register_buffer(client, &output_dmabuf);

    // 6. Send response with FD
    struct blur_render_response resp = {
        .header = { .status = 0, .sequence = req->header.sequence },
        .output_buffer_id = output_buffer_id,
    };

    send_response_with_fd(client->sockfd, &resp, sizeof(resp), output_dmabuf.fd[0]);

    return 0;
}
```

### 5. Destroy Blur Node

**Purpose:** Clean up blur node and associated resources

**Request:**
```c
struct blur_destroy_node_request {
    struct blur_request_header header;  // opcode = DESTROY_NODE
    uint32_t blur_id;
};
```

**Response:**
```c
struct blur_destroy_node_response {
    struct blur_response_header header;  // status = 0 on success
};
```

---

## Virtual Scene Graph

### Purpose

Track blur nodes and buffers without understanding compositor's actual scene graph.

**Design Philosophy:**
- Daemon doesn't know about windows, layers, or compositor internals
- Compositor manages mapping: window → blur_id
- Daemon manages: blur_id → blur state

### Data Structures

```c
// blur_node.c

// Blur node (represents one blur region)
struct blur_node {
    uint32_t blur_id;              // Unique ID
    uint32_t client_id;            // Owner client
    int32_t width, height;         // Dimensions
    struct wlblur_params params;   // Blur parameters
    bool active;
};

// Buffer registry (tracks DMA-BUF imports)
struct buffer_entry {
    uint32_t buffer_id;            // Unique ID
    uint32_t client_id;            // Owner client
    struct wlblur_dmabuf dmabuf;   // DMA-BUF attributes
    wlblur_texture_t* texture;     // libwlblur texture handle
    time_t last_used;              // For garbage collection
};

// Client state
struct client_state {
    uint32_t client_id;
    int sockfd;
    char compositor_name[32];

    // Resource tracking
    uint32_t* blur_nodes;          // Array of blur_id
    uint32_t blur_node_count;
    uint32_t* buffers;             // Array of buffer_id
    uint32_t buffer_count;

    // Quotas
    uint32_t max_blur_nodes;       // Default: 256
    uint32_t max_buffers;          // Default: 1024

    // Statistics
    uint64_t requests_served;
    uint64_t bytes_transferred;
};
```

### Node Lifecycle

```
┌─────────────────┐
│   CREATE_NODE   │
│  (compositor)   │
└────────┬────────┘
         │
         ↓
┌─────────────────┐
│ Blur Node       │
│ • blur_id = 1   │
│ • client_id = 1 │
│ • params = {}   │
│ • active = true │
└────────┬────────┘
         │
         │  Multiple RENDER requests
         │  ↓
         │  ┌──────────────┐
         └─►│ IMPORT_DMABUF│───┐
            └──────────────┘   │
                               ↓
            ┌──────────────┐   │
            │ Buffer Entry │◄──┘
            │ • buffer_id  │
            │ • dmabuf FD  │
            │ • texture    │
            └──────┬───────┘
                   │
                   ↓
            ┌──────────────┐
            │   RENDER     │
            │ (blur exec)  │
            └──────┬───────┘
                   │
                   ↓
            ┌──────────────┐
            │ Output Buffer│
            │ • buffer_id  │
            │ • dmabuf FD  │
            └──────┬───────┘
                   │
                   ↓  Compositor imports

         ┌──────────────┐
         │DESTROY_NODE  │
         │(compositor)  │
         └──────┬───────┘
                │
                ↓
         ┌──────────────┐
         │ Cleanup      │
         │ • Free node  │
         │ • Release    │
         │   buffers    │
         │ • Close FDs  │
         └──────────────┘
```

---

## Resource Management

### Client Quotas

```c
#define MAX_BLUR_NODES_PER_CLIENT    256
#define MAX_BUFFERS_PER_CLIENT       1024
#define MAX_CLIENT_CONNECTIONS       16
```

**Enforcement:**
```c
int create_blur_node(struct client_state* client, int width, int height) {
    if (client->blur_node_count >= client->max_blur_nodes) {
        return -ENOSPC;  // Quota exceeded
    }

    // ... create node ...

    client->blur_nodes[client->blur_node_count++] = blur_id;
    return blur_id;
}
```

### Buffer Garbage Collection

```c
// Automatically release buffers after 5 seconds of inactivity
void garbage_collect_buffers(void) {
    time_t now = time(NULL);

    for (each buffer in buffer_registry) {
        if (now - buffer->last_used > 5) {
            wlblur_texture_destroy(buffer->texture);
            close(buffer->dmabuf.fd[0]);  // Close FD
            free(buffer);
        }
    }
}
```

### Client Disconnect Cleanup

```c
void cleanup_client(struct client_state* client) {
    // Destroy all blur nodes
    for (int i = 0; i < client->blur_node_count; i++) {
        destroy_blur_node(client->blur_nodes[i]);
    }

    // Release all buffers
    for (int i = 0; i < client->buffer_count; i++) {
        release_buffer(client->buffers[i]);
    }

    // Close socket
    close(client->sockfd);

    // Free client state
    free(client);
}
```

---

## Main Event Loop

### epoll-Based Architecture

```c
// main.c

int main(int argc, char* argv[]) {
    // 1. Create Unix socket
    int listen_fd = create_unix_socket("/run/user/$UID/wlblur.sock");

    // 2. Create epoll instance
    int epoll_fd = epoll_create1(0);

    // 3. Add listen socket to epoll
    struct epoll_event ev = {
        .events = EPOLLIN,
        .data.fd = listen_fd,
    };
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, listen_fd, &ev);

    // 4. Initialize libwlblur context
    wlblur_context_t* ctx = wlblur_context_create();

    // 5. Main event loop
    while (running) {
        struct epoll_event events[32];
        int nfds = epoll_wait(epoll_fd, events, 32, 1000);  // 1s timeout

        for (int i = 0; i < nfds; i++) {
            if (events[i].data.fd == listen_fd) {
                // New client connection
                int client_fd = accept(listen_fd, NULL, NULL);
                struct client_state* client = create_client(client_fd);

                // Add to epoll
                ev.events = EPOLLIN;
                ev.data.ptr = client;
                epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);

            } else {
                // Client message
                struct client_state* client = events[i].data.ptr;
                handle_client_message(client);
            }
        }

        // Periodic cleanup
        garbage_collect_buffers();
    }

    // Cleanup
    wlblur_context_destroy(ctx);
    close(listen_fd);
    close(epoll_fd);

    return 0;
}
```

---

## Configuration

### Config File

**Location:** `~/.config/wlblur/config.ini`

```ini
[daemon]
socket_path = /run/user/1000/wlblur.sock
max_clients = 16
log_level = info

[defaults]
blur_algorithm = kawase
blur_passes = 2
blur_offset = 1.25
vibrancy = 0.2
brightness = 0.0
contrast = 1.0
noise = 0.01

[quotas]
max_blur_nodes_per_client = 256
max_buffers_per_client = 1024
buffer_gc_timeout_seconds = 5
```

### Config Loading

```c
// config.c
struct daemon_config {
    char socket_path[256];
    int max_clients;
    struct wlblur_params default_params;
    struct {
        uint32_t max_blur_nodes;
        uint32_t max_buffers;
        uint32_t gc_timeout;
    } quotas;
};

struct daemon_config* load_config(const char* path) {
    // Parse INI file
    // Apply defaults for missing values
    // Validate ranges
    return config;
}
```

---

## Error Handling

### Error Codes

```c
enum blur_error {
    BLUR_ERROR_NONE = 0,
    BLUR_ERROR_INVALID_MAGIC = -1,
    BLUR_ERROR_INVALID_VERSION = -2,
    BLUR_ERROR_INVALID_CLIENT_ID = -3,
    BLUR_ERROR_INVALID_BLUR_ID = -4,
    BLUR_ERROR_INVALID_BUFFER_ID = -5,
    BLUR_ERROR_QUOTA_EXCEEDED = -6,
    BLUR_ERROR_DMABUF_IMPORT_FAILED = -7,
    BLUR_ERROR_RENDER_FAILED = -8,
    BLUR_ERROR_OUT_OF_MEMORY = -9,
};
```

### Error Responses

```c
void send_error_response(int sockfd, uint32_t sequence, int error_code) {
    struct blur_response_header resp = {
        .magic = 0x424C5552,
        .version = 1,
        .sequence = sequence,
        .status = error_code,
        .payload_size = 0,
    };

    send(sockfd, &resp, sizeof(resp), 0);
}
```

---

## Deployment

### systemd User Service

**File:** `~/.config/systemd/user/wlblur.service`

```ini
[Unit]
Description=wlblur Daemon - Compositor-Agnostic Blur Service
Documentation=man:wlblurd(1)

[Service]
Type=simple
ExecStart=/usr/bin/wlblurd
Restart=on-failure
RestartSec=5s

# Security hardening
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=read-only

[Install]
WantedBy=graphical-session.target
```

**Activation:**
```bash
systemctl --user enable wlblur.service
systemctl --user start wlblur.service
```

### Socket Activation (Future)

```ini
[Unit]
Description=wlblur Socket

[Socket]
ListenStream=/run/user/%U/wlblur.sock
SocketMode=0600

[Install]
WantedBy=sockets.target
```

---

## Performance Monitoring

### Statistics Collection

```c
struct daemon_stats {
    uint64_t total_requests;
    uint64_t total_renders;
    uint64_t total_bytes_transferred;
    uint64_t total_blur_time_us;  // Microseconds
    uint32_t active_clients;
    uint32_t active_blur_nodes;
    uint32_t active_buffers;
};

// Export via D-Bus or Unix socket query
```

### Logging

```c
enum log_level {
    LOG_ERROR = 0,
    LOG_WARN = 1,
    LOG_INFO = 2,
    LOG_DEBUG = 3,
};

void log_message(enum log_level level, const char* fmt, ...) {
    if (level > config.log_level) return;

    // Timestamp + level + message
    fprintf(stderr, "[%s] %s: %s\n", timestamp(), level_str(level), message);
}
```

---

## Security Considerations

### Input Validation

```c
bool validate_request(struct blur_request_header* header) {
    // Magic number
    if (header->magic != 0x424C5552) {
        return false;
    }

    // Protocol version
    if (header->version != 1) {
        return false;
    }

    // Opcode range
    if (header->opcode > BLUR_OP_PING) {
        return false;
    }

    // Payload size (max 16 KB)
    if (header->payload_size > 16384) {
        return false;
    }

    return true;
}

bool validate_dmabuf(struct blur_import_dmabuf_request* req) {
    // Dimensions
    if (req->width < 1 || req->width > 16384 ||
        req->height < 1 || req->height > 16384) {
        return false;
    }

    // Format whitelist
    if (req->format != DRM_FORMAT_ARGB8888 &&
        req->format != DRM_FORMAT_XRGB8888 &&
        req->format != DRM_FORMAT_ABGR8888 &&
        req->format != DRM_FORMAT_XBGR8888) {
        return false;
    }

    // Plane count
    if (req->num_planes < 1 || req->num_planes > 4) {
        return false;
    }

    return true;
}
```

### FD Leak Prevention

```c
// Always close FDs after EGL import
int fd = recv_dmabuf_fd(sockfd);
wlblur_texture_t* tex = wlblur_import_dmabuf(ctx, &dmabuf);
close(fd);  // FD no longer needed, EGL has reference
```

---

## References

### Related Docs
- [00-overview.md](00-overview.md) - System architecture
- [01-libwlblur.md](01-libwlblur.md) - Library internals
- [03-integration.md](03-integration.md) - Compositor patterns

### Investigation Sources
- `docs/post-investigation/blur-daemon-approach.md` - IPC design rationale
- `docs/investigation/scenefx-investigation/` - Scene graph patterns

---

**Next:** [Compositor Integration (03-integration.md)](03-integration.md)
