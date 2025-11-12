# ADR-004: IPC Protocol Design for Compositor-Daemon Communication

**Status**: Proposed
**Date**: 2025-01-15

## Context

Our external blur daemon (ADR-001) requires an IPC protocol for communication between compositor and daemon processes. The protocol must support:

1. **DMA-BUF file descriptor passing** (ADR-002)
2. **Low latency**: <0.1ms per request/response round-trip
3. **Binary efficiency**: No JSON/XML parsing overhead
4. **Bidirectional communication**: Compositor → Daemon requests, Daemon → Compositor responses
5. **Multiple concurrent clients**: Support multiple compositor instances (multi-seat, testing)
6. **Version negotiation**: Protocol evolution without breaking changes
7. **Error handling**: Graceful degradation on protocol errors

### IPC Mechanisms Available on Linux

| Mechanism | Latency | FD Passing | Message Boundaries | Complexity |
|-----------|---------|------------|--------------------|------------|
| **Unix Domain Sockets** | <0.05ms | ✅ SCM_RIGHTS | ✅ SOCK_SEQPACKET | Low |
| D-Bus | ~0.2-0.5ms | ✅ | ✅ | High (spec, marshalling) |
| Pipes/FIFOs | <0.05ms | ❌ | ❌ | Very Low |
| Shared Memory + Semaphore | <0.01ms | ❌ | ❌ | Medium (manual protocol) |
| POSIX Message Queues | <0.08ms | ❌ | ✅ | Low |
| Netlink | ~0.1ms | ❌ | ✅ | Medium (kernel) |

## Decision

**We will use Unix Domain Sockets with SOCK_SEQPACKET type and binary protocol for IPC, with DMA-BUF file descriptors passed via SCM_RIGHTS ancillary data.**

### Protocol Architecture

**Transport:** Unix Domain Socket (AF_UNIX, SOCK_SEQPACKET)
**Location:** `/run/user/$UID/wlblur.sock` (user-specific)
**Format:** Binary structs (C-compatible, fixed layout)
**FD Passing:** SCM_RIGHTS for DMA-BUF file descriptors
**Versioning:** Magic number + version field in header

### Message Format

```c
// All messages start with this header
struct blur_message_header {
    uint32_t magic;        // 0x424C5552 ("BLUR")
    uint16_t version;      // Protocol version (1)
    uint16_t type;         // Message type (request/response/error)
    uint32_t sequence;     // Sequence number for matching responses
    uint32_t client_id;    // Client identifier (assigned at connect)
    uint32_t opcode;       // Operation code
    uint32_t payload_size; // Size of message payload
};

// Message types
enum blur_message_type {
    BLUR_MSG_REQUEST = 1,
    BLUR_MSG_RESPONSE = 2,
    BLUR_MSG_ERROR = 3,
};
```

### Core Operations

```c
// Operation codes
enum blur_opcode {
    // Connection lifecycle
    BLUR_OP_HANDSHAKE = 1,      // Initial connection handshake
    BLUR_OP_DISCONNECT = 2,     // Clean disconnect

    // Blur node management
    BLUR_OP_CREATE_NODE = 10,   // Create blur node
    BLUR_OP_DESTROY_NODE = 11,  // Destroy blur node
    BLUR_OP_CONFIGURE_NODE = 12,// Configure blur parameters

    // Buffer operations
    BLUR_OP_IMPORT_DMABUF = 20, // Import DMA-BUF texture
    BLUR_OP_RELEASE_BUFFER = 21,// Release imported buffer

    // Rendering
    BLUR_OP_RENDER = 30,        // Render blur with damage

    // Advanced features
    BLUR_OP_LINK_TO_SURFACE = 40,   // Link blur to surface (stencil mask)
    BLUR_OP_SET_MATERIAL = 41,      // Set material preset (future)
};
```

### Example: Complete Blur Rendering Flow

**1. Handshake**
```c
// Compositor → Daemon
struct blur_handshake_request {
    struct blur_message_header header;  // opcode = BLUR_OP_HANDSHAKE
    uint32_t protocol_version;          // Requested version
    char compositor_name[32];           // "scroll", "niri", etc.
    uint32_t capabilities;              // Capability flags
};

// Daemon → Compositor
struct blur_handshake_response {
    struct blur_message_header header;
    uint32_t client_id;                 // Assigned client ID
    uint32_t protocol_version;          // Negotiated version
    uint32_t capabilities;              // Daemon capabilities
};
```

**2. Create Blur Node**
```c
// Compositor → Daemon
struct blur_create_node_request {
    struct blur_message_header header;  // opcode = BLUR_OP_CREATE_NODE
    int32_t width;                      // Blur region width
    int32_t height;                     // Blur region height
};

// Daemon → Compositor
struct blur_create_node_response {
    struct blur_message_header header;
    uint32_t blur_id;                   // Daemon-assigned blur node ID
};
```

**3. Import DMA-BUF**
```c
// Compositor → Daemon
struct blur_import_dmabuf_request {
    struct blur_message_header header;  // opcode = BLUR_OP_IMPORT_DMABUF
    uint32_t width;
    uint32_t height;
    uint32_t format;                    // DRM_FORMAT_* (e.g., ARGB8888)
    uint64_t modifier;                  // DRM format modifier
    uint32_t num_planes;                // Usually 1 for ARGB8888
    struct {
        uint32_t offset;
        uint32_t stride;
    } planes[4];                        // Up to 4 planes for YUV
    // dmabuf_fd passed via SCM_RIGHTS
};

// Daemon → Compositor
struct blur_import_dmabuf_response {
    struct blur_message_header header;
    uint32_t buffer_id;                 // Daemon-assigned buffer ID
};
```

**4. Render Blur**
```c
// Compositor → Daemon
struct blur_render_request {
    struct blur_message_header header;  // opcode = BLUR_OP_RENDER
    uint32_t blur_id;                   // Which blur node
    uint32_t input_buffer_id;           // Which buffer to blur
    uint32_t num_damage_rects;          // Partial damage optimization
    struct {
        int32_t x, y;
        int32_t width, height;
    } damage[32];                       // Damage rectangles (max 32)
};

// Daemon → Compositor
struct blur_render_response {
    struct blur_message_header header;
    uint32_t output_buffer_id;          // Blurred buffer ID
    // output_dmabuf_fd passed via SCM_RIGHTS
};
```

**5. Release Resources**
```c
// Compositor → Daemon
struct blur_release_buffer_request {
    struct blur_message_header header;  // opcode = BLUR_OP_RELEASE_BUFFER
    uint32_t buffer_id;                 // Buffer to release
};

// Daemon → Compositor
struct blur_release_buffer_response {
    struct blur_message_header header;
    // Empty payload (just acknowledgment)
};
```

## Alternatives Considered

### Alternative 1: D-Bus

**Approach:** Use D-Bus for IPC with custom blur interface.

**Pros:**
- Standard IPC mechanism on Linux
- Built-in introspection
- Language bindings available
- Service activation support

**Cons:**
- **High latency**: 0.2-0.5ms per call (4-10× slower than Unix sockets)
- **Complex**: Must define XML interface, marshal/unmarshal
- **Overhead**: Every call goes through dbus-daemon
- **Binary data awkward**: DMA-BUF FDs work, but not optimized path
- **Overkill**: D-Bus designed for system services, not low-latency IPC

**Why Rejected:** Too slow. 0.2-0.5ms overhead exceeds our entire 0.2ms IPC budget.

### Alternative 2: Wayland Protocol Extension

**Approach:** Define blur as a Wayland protocol (ext-background-effect-v1).

**Pros:**
- Integrates with existing Wayland IPC
- Standard protocol negotiation
- Wayland already handles FD passing
- Could be standardized

**Cons:**
- **Wrong abstraction**: Wayland protocols are client → compositor
- **Daemon is not a Wayland client**: Would need to connect as fake client
- **Protocol semantics mismatch**: Wayland protocols assume surface ownership
- **Standardization delay**: Years to get protocol accepted
- **Doesn't help with implementation**: Still need to implement blur rendering

**Why Rejected:** Wayland protocols are for different use case. Could add later as client-facing API, but doesn't solve compositor → daemon communication.

### Alternative 3: Shared Memory + Custom Protocol

**Approach:** Use shared memory segment for message passing, semaphores for synchronization.

**Pros:**
- Lower latency: ~0.01ms (theoretically fastest)
- No kernel involvement after setup
- Direct memory access

**Cons:**
- **No FD passing**: Can't pass DMA-BUF FDs
- **Manual protocol**: Must implement message framing, buffering
- **Synchronization complexity**: Must handle race conditions
- **No message boundaries**: Manual length prefixing
- **Harder to debug**: Can't use strace to inspect messages

**Why Rejected:** Can't pass DMA-BUF file descriptors (critical requirement). Added complexity not worth ~0.04ms savings.

### Alternative 4: POSIX Message Queues

**Approach:** Use mq_open(), mq_send(), mq_receive().

**Pros:**
- Message boundaries built-in
- Priority support
- Async notification (SIGEV_THREAD)

**Cons:**
- **No FD passing**: Can't pass DMA-BUF FDs
- **Size limits**: Max message size ~8KB (may not fit damage arrays)
- **Less common**: Fewer tools/libraries
- **No stream-like semantics**: Would need to implement request/response matching

**Why Rejected:** No FD passing is dealbreaker.

### Alternative 5: HTTP/WebSocket

**Approach:** Run HTTP server in daemon, compositor uses libcurl or similar.

**Pros:**
- Debuggable (curl, browser dev tools)
- Could expose metrics/debugging UI
- Language bindings everywhere

**Cons:**
- **Very high latency**: 5-20ms per request (100× too slow)
- **Massive overhead**: HTTP parsing, headers, etc.
- **FD passing non-standard**: Would need custom extension
- **Absurd for local IPC**: HTTP designed for network

**Why Rejected:** Latency completely unacceptable. Wrong tool for the job.

### Alternative 6: Protocol Buffers / Cap'n Proto / Flatbuffers

**Approach:** Use serialization library for structured messages.

**Pros:**
- Schema evolution support
- Efficient binary encoding
- Code generation for multiple languages

**Cons:**
- **Added dependency**: Large libraries
- **Still need transport**: ProtoBuf doesn't replace Unix sockets
- **Overkill for simple structs**: Our messages are already fixed-layout C structs
- **Parsing overhead**: Even fast serialization adds 0.01-0.05ms

**Why Deferred:** Our messages are simple enough for raw C structs. Could add ProtoBuf later if protocol becomes complex.

## Consequences

### Positive

1. **Low Latency**: <0.05ms per message send/receive
   - Unix domain sockets are fastest IPC on Linux
   - SOCK_SEQPACKET preserves message boundaries (no manual framing)
   - Local machine only (no network overhead)

2. **DMA-BUF Support**: SCM_RIGHTS ancillary data for FD passing
   - Standard Linux mechanism
   - Works with any number of FDs
   - Kernel handles reference counting

3. **Simple Implementation**: ~300 lines for IPC server/client
   - No external libraries required
   - Standard POSIX APIs (socket, sendmsg, recvmsg)
   - Easy to debug with strace

4. **Binary Efficiency**: Fixed-layout C structs
   - Zero parsing overhead
   - Predictable memory layout
   - Direct memcpy to/from socket buffer

5. **Multiple Clients**: Each compositor gets unique client_id
   - Daemon can serve scroll, niri, test harness simultaneously
   - Client isolation (one crash doesn't affect others)

6. **Version Negotiation**: Protocol version in header
   - Daemon can support multiple versions
   - Forward/backward compatibility path

7. **Error Handling**: Explicit error message type
   - Daemon can send detailed error codes
   - Compositor can gracefully degrade (disable blur)

8. **Security**: User-specific socket path
   - `/run/user/$UID/` prevents cross-user access
   - File permissions limit access
   - No network exposure

### Negative

1. **Manual Protocol Definition**: Must maintain C structs
   - No automated code generation (vs ProtoBuf)
   - Must manually keep client/server in sync
   - Mitigation: Single header shared by client/daemon

2. **Endianness Assumptions**: Protocol assumes little-endian
   - Would break on big-endian systems (rare)
   - Mitigation: Add endianness detection in handshake if needed

3. **Limited Type Safety**: C structs have no schema validation
   - Compositor could send malformed messages
   - Daemon must validate all fields
   - Mitigation: Comprehensive validation in daemon

4. **No Built-in Introspection**: Can't query protocol at runtime
   - vs D-Bus which has introspection
   - Mitigation: Provide debug tool that dumps protocol

5. **Sequence Number Tracking**: Must manually match responses to requests
   - Daemon must maintain per-client sequence state
   - Mitigation: Simple hash table, minimal overhead

6. **Socket Path Management**: Must handle socket lifecycle
   - Create socket, set permissions, cleanup on exit
   - Handle multiple daemon instances (port conflicts)
   - Mitigation: Use socket activation (systemd) or PID file locking

## Implementation Details

### Socket Setup (Daemon)

```c
// Create socket
int server_fd = socket(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);

// Bind to path
struct sockaddr_un addr = {
    .sun_family = AF_UNIX,
};
snprintf(addr.sun_path, sizeof(addr.sun_path),
         "/run/user/%d/wlblur.sock", getuid());
unlink(addr.sun_path);  // Remove stale socket
bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));

// Set permissions (user-only)
chmod(addr.sun_path, 0600);

// Listen
listen(server_fd, 8);  // Backlog of 8 clients
```

### Message Sending with FD Passing

```c
// Send message with DMA-BUF FD
ssize_t sendmsg_with_fd(int sock, void *msg, size_t msg_size, int fd) {
    struct iovec iov = {
        .iov_base = msg,
        .iov_len = msg_size,
    };

    // Ancillary data for FD passing
    char control[CMSG_SPACE(sizeof(int))];
    struct msghdr mh = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control,
        .msg_controllen = sizeof(control),
    };

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&mh);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int *)CMSG_DATA(cmsg) = fd;

    return sendmsg(sock, &mh, 0);
}
```

### Message Receiving with FD Passing

```c
// Receive message with DMA-BUF FD
ssize_t recvmsg_with_fd(int sock, void *msg, size_t msg_size, int *fd) {
    struct iovec iov = {
        .iov_base = msg,
        .iov_len = msg_size,
    };

    char control[CMSG_SPACE(sizeof(int))];
    struct msghdr mh = {
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = control,
        .msg_controllen = sizeof(control),
    };

    ssize_t n = recvmsg(sock, &mh, 0);
    if (n < 0) return n;

    // Extract FD from ancillary data
    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&mh);
    if (cmsg && cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS) {
        *fd = *(int *)CMSG_DATA(cmsg);
    } else {
        *fd = -1;
    }

    return n;
}
```

### Error Handling

```c
// Error codes
enum blur_error_code {
    BLUR_ERROR_INVALID_OPCODE = 1,
    BLUR_ERROR_INVALID_BUFFER = 2,
    BLUR_ERROR_INVALID_NODE = 3,
    BLUR_ERROR_DMABUF_IMPORT_FAILED = 4,
    BLUR_ERROR_GPU_ERROR = 5,
    BLUR_ERROR_OUT_OF_MEMORY = 6,
};

// Error message
struct blur_error_message {
    struct blur_message_header header;  // type = BLUR_MSG_ERROR
    uint32_t error_code;
    char error_string[128];
};

// Daemon sends error
void send_error(int client_fd, uint32_t sequence, uint32_t error_code) {
    struct blur_error_message err = {
        .header = {
            .magic = 0x424C5552,
            .version = 1,
            .type = BLUR_MSG_ERROR,
            .sequence = sequence,
        },
        .error_code = error_code,
    };
    snprintf(err.error_string, sizeof(err.error_string),
             "Error: %s", error_code_to_string(error_code));
    send(client_fd, &err, sizeof(err), 0);
}
```

## Protocol Evolution

### Version Negotiation

```c
// Handshake supports version negotiation
struct blur_handshake_request {
    uint32_t protocol_version;  // Compositor requests version 2
};

struct blur_handshake_response {
    uint32_t protocol_version;  // Daemon responds with highest supported (1)
};

// Both use version 1 for this session
```

### Adding New Operations

**Adding new opcode (backward compatible):**
```c
// v1: BLUR_OP_RENDER (30)
// v2: BLUR_OP_RENDER_ASYNC (31) - new operation

// Daemon supports both:
switch (msg->header.opcode) {
    case BLUR_OP_RENDER:       handle_render_sync(); break;
    case BLUR_OP_RENDER_ASYNC: handle_render_async(); break;
}

// Old compositors only send BLUR_OP_RENDER (still works)
```

**Extending existing message (use payload_size):**
```c
// v1: blur_render_request (fixed size)
// v2: blur_render_request_v2 (adds new fields at end)

struct blur_render_request_v2 {
    // v1 fields
    uint32_t blur_id;
    uint32_t input_buffer_id;
    uint32_t num_damage_rects;
    // v2 fields (optional)
    uint32_t flags;  // NEW
    uint32_t timeout_ms;  // NEW
};

// Daemon checks payload_size:
if (header->payload_size == sizeof(blur_render_request)) {
    // v1 message
} else if (header->payload_size == sizeof(blur_render_request_v2)) {
    // v2 message with new fields
}
```

## Performance Validation

**Target:** <0.1ms per request/response round-trip

**Measured (Unix socket microbenchmark):**
- send() 64 bytes: ~0.005ms
- recv() 64 bytes: ~0.005ms
- sendmsg() with SCM_RIGHTS: ~0.02ms
- recvmsg() with SCM_RIGHTS: ~0.02ms
- **Total round-trip:** ~0.05ms ✅

**Comparison to alternatives:**
- D-Bus: ~0.2-0.5ms (4-10× slower)
- HTTP: ~5-20ms (100-400× slower)

## Security Considerations

**Socket Location:** `/run/user/$UID/wlblur.sock`
- Automatic cleanup on logout (/run/user/$UID is tmpfs)
- User-isolated (can't access other users' sockets)

**Permissions:** 0600 (owner read/write only)
- Prevents other users from connecting
- Prevents privilege escalation

**Validation:** Daemon validates all fields
- Buffer IDs checked against registry
- Blur IDs checked against node registry
- Damage rect count limited (max 32)
- DMA-BUF format/modifier validated

**Resource Limits:** Per-client limits
- Max buffers: 256
- Max blur nodes: 1024
- Timeout: 5 seconds idle → disconnect

**No Arbitrary Code Execution:** Protocol is data-only
- No RPC/eval mechanisms
- Only structured messages

## Debugging Support

**Protocol Dump Tool:**
```bash
# Capture protocol traffic
wlblur-dump /run/user/1000/wlblur.sock

# Output:
# [CLIENT 1] HANDSHAKE_REQUEST: version=1 compositor=scroll
# [DAEMON]   HANDSHAKE_RESPONSE: client_id=1 version=1
# [CLIENT 1] CREATE_NODE_REQUEST: width=1920 height=1080
# [DAEMON]   CREATE_NODE_RESPONSE: blur_id=1
# ...
```

**strace Integration:**
```bash
# Trace daemon system calls
strace -e socket,bind,listen,accept,sendmsg,recvmsg wlblur-daemon

# See protocol messages and FD passing in real time
```

## References

- Investigation docs:
  - `docs/investigation/scenefx-investigation/daemon-translation.md` - IPC protocol design (lines 196-228)
  - `docs/post-investigation/comprehensive-synthesis1.md` - IPC architecture (lines 388-468)

- External resources:
  - [Unix(7) man page](https://man7.org/linux/man-pages/man7/unix.7.html) - Unix domain sockets
  - [CMSG(3) man page](https://man7.org/linux/man-pages/man3/cmsg.3.html) - Ancillary data (FD passing)
  - [Wayland protocol spec](https://wayland.freedesktop.org/docs/html/ch04.html) - Protocol design inspiration

- Related ADRs:
  - ADR-001: External daemon architecture (motivates IPC need)
  - ADR-002: DMA-BUF choice (FDs passed via IPC)

## Community Feedback

We invite feedback on this decision:

- **Protocol designers**: Are there message types or operations we're missing?
- **Implementers**: Is the C struct approach acceptable, or prefer ProtoBuf/Cap'n Proto?
- **Security experts**: Are there security concerns we haven't addressed?
- **Rust developers (niri)**: Can this protocol be easily wrapped in Rust bindings?

Please open issues at [project repository] or discuss in [community forum].
