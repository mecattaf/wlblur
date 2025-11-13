# wlblur IPC Protocol Specification

**Version:** 1.0
**Protocol Type:** Binary over Unix Domain Socket
**Transport:** SCM_RIGHTS for file descriptor passing
**Last Updated:** 2025-01-15

---

## Overview

The wlblur IPC protocol enables Wayland compositors to offload blur rendering to an external daemon process. This provides:

- **Process isolation**: Daemon crashes don't affect compositor stability
- **Zero-copy GPU memory sharing**: DMA-BUF file descriptors passed via SCM_RIGHTS
- **Multi-compositor support**: Single daemon can serve multiple compositor types
- **Independent evolution**: Daemon updates (new algorithms, optimizations) without compositor rebuilds

### Architecture

```
┌──────────────────────────┐         ┌─────────────────────────────┐
│   Wayland Compositor     │         │     Blur Daemon             │
│                          │         │                             │
│  • Scene graph           │         │  • Virtual scene graph      │
│  • Window management     │         │  • Blur node registry       │
│  • Standard rendering    │         │  • GL/Vulkan renderer       │
│                          │         │  • Shader pipeline          │
│  ┌────────────────────┐  │         │  ┌───────────────────────┐  │
│  │  wlblur IPC Client │  │◄───────►│  │  IPC Server           │  │
│  └────────────────────┘  │  Unix   │  └───────────────────────┘  │
│                          │  Socket │                             │
│  ┌────────────────────┐  │         │  ┌───────────────────────┐  │
│  │  DMA-BUF Exporter  │──┼────────►│  │  DMA-BUF Importer     │  │
│  └────────────────────┘  │SCM_RIGHTS│  └───────────────────────┘  │
└──────────────────────────┘         └─────────────────────────────┘
```

---

## Socket Path

### Location

**Default path:** `/run/user/$UID/wlblur.sock`

- `$UID`: User ID of the running session
- Created by daemon on startup
- Removed on daemon shutdown

### Permissions

- **Socket file mode:** `0700` (owner read/write/execute only)
- **Socket ownership:** Must match connecting client's UID
- **Directory:** `/run/user/$UID/` (created by systemd user session)

### Discovery

Clients should:
1. Check environment variable `WLBLUR_SOCKET` first
2. Fall back to default path `/run/user/$UID/wlblur.sock`
3. Fail gracefully if socket doesn't exist (daemon not running)

---

## Message Format

All messages use binary C structures with native byte order (host endianness). Fields are aligned according to C struct packing rules.

### Request Header

All client requests begin with this header:

```c
struct wlblur_request_header {
    uint32_t protocol_version;  // Currently 1
    uint32_t op;                // Operation code (see Operations section)
    uint32_t request_id;        // Client-assigned ID for matching responses
    uint32_t payload_size;      // Size of operation-specific payload in bytes
};
```

**Fields:**
- `protocol_version`: Must be `1` for this specification. Future versions may increment.
- `op`: Operation code from `WLBLUR_OP_*` constants
- `request_id`: Arbitrary client-assigned ID. Daemon echoes in response for correlation.
- `payload_size`: Number of bytes following header. May be 0 for operations without payload.

### Response Header

All daemon responses begin with this header:

```c
struct wlblur_response_header {
    uint32_t request_id;        // Echoed from request
    int32_t  error_code;        // 0 = success, non-zero = error (see Error Codes)
    uint32_t payload_size;      // Size of operation-specific payload in bytes
};
```

**Fields:**
- `request_id`: Copied from request for correlation
- `error_code`: 0 indicates success. Non-zero values are error codes (see Error Codes section)
- `payload_size`: Number of bytes following header

---

## Operations

### WLBLUR_OP_CREATE_NODE (1)

**Purpose:** Allocate a new blur node in the daemon's virtual scene graph.

**Request Structure:**
```c
struct wlblur_create_node_request {
    struct wlblur_request_header header;
    uint32_t parent_id;         // Parent scene node ID (0 = root)
    int32_t  width;             // Blur region width in pixels
    int32_t  height;            // Blur region height in pixels
};
```

**Response Structure:**
```c
struct wlblur_create_node_response {
    struct wlblur_response_header header;
    uint32_t node_id;           // Daemon-assigned blur node ID
};
```

**Semantics:**
- Creates a new blur node tracked by the daemon
- `node_id` must be used in subsequent operations on this node
- Node persists until explicitly destroyed via `WLBLUR_OP_DESTROY_NODE`
- Default parameters: strength=1.0, alpha=1.0, corner_radius=0

**Error Codes:**
- `WLBLUR_ERROR_OUT_OF_MEMORY`: Failed to allocate node
- `WLBLUR_ERROR_INVALID_DIMENSIONS`: width/height <= 0 or > MAX_TEXTURE_SIZE
- `WLBLUR_ERROR_MAX_NODES_EXCEEDED`: Client has exceeded per-client node limit

---

### WLBLUR_OP_DESTROY_NODE (2)

**Purpose:** Destroy a blur node and free associated resources.

**Request Structure:**
```c
struct wlblur_destroy_node_request {
    struct wlblur_request_header header;
    uint32_t node_id;           // Blur node ID to destroy
};
```

**Response Structure:**
```c
struct wlblur_destroy_node_response {
    struct wlblur_response_header header;
};
```

**Semantics:**
- Frees daemon-side resources for this node
- Invalidates `node_id` (subsequent operations will fail)
- Unlinks any associated mask buffers
- Does not destroy imported buffers (use `WLBLUR_OP_RELEASE_BUFFER`)

**Error Codes:**
- `WLBLUR_ERROR_INVALID_NODE`: Node ID doesn't exist or already destroyed

---

### WLBLUR_OP_IMPORT_DMABUF (3)

**Purpose:** Import a DMA-BUF into the daemon as a texture.

**Request Structure:**
```c
struct wlblur_import_dmabuf_request {
    struct wlblur_request_header header;
    uint32_t width;             // Texture width in pixels
    uint32_t height;            // Texture height in pixels
    uint32_t format;            // DRM FourCC format (e.g., DRM_FORMAT_ARGB8888)
    uint8_t  n_planes;          // Number of planes (1-4)
    uint32_t offsets[4];        // Plane offsets in bytes
    uint32_t strides[4];        // Plane strides in bytes
    uint64_t modifier;          // DRM format modifier
    // File descriptors sent via SCM_RIGHTS ancillary data
};
```

**Ancillary Data:**
- **Type:** `SCM_RIGHTS`
- **Contents:** `n_planes` file descriptors (one per DMA-BUF plane)
- **Order:** fd[0] for plane 0, fd[1] for plane 1, etc.

**Response Structure:**
```c
struct wlblur_import_dmabuf_response {
    struct wlblur_response_header header;
    uint32_t buffer_id;         // Daemon-assigned buffer ID
};
```

**Semantics:**
- Imports DMA-BUF as EGLImage and creates GL texture
- Daemon takes ownership of file descriptors (closes after import)
- `buffer_id` used to reference this texture in render operations
- Buffer persists until `WLBLUR_OP_RELEASE_BUFFER`
- Reference counted (multiple nodes can use same buffer)

**Error Codes:**
- `WLBLUR_ERROR_DMABUF_IMPORT_FAILED`: EGL/GL import failed
- `WLBLUR_ERROR_UNSUPPORTED_FORMAT`: Format/modifier not supported
- `WLBLUR_ERROR_INVALID_DMABUF`: Invalid dimensions or plane configuration
- `WLBLUR_ERROR_OUT_OF_MEMORY`: Failed to allocate buffer object

---

### WLBLUR_OP_RELEASE_BUFFER (4)

**Purpose:** Decrement reference count on imported buffer. Destroys when count reaches zero.

**Request Structure:**
```c
struct wlblur_release_buffer_request {
    struct wlblur_request_header header;
    uint32_t buffer_id;         // Buffer ID to release
};
```

**Response Structure:**
```c
struct wlblur_release_buffer_response {
    struct wlblur_response_header header;
};
```

**Semantics:**
- Decrements internal reference count
- If count reaches 0: destroys GL texture, EGLImage, and buffer object
- This is asynchronous cleanup (don't wait for response unless needed)
- Idempotent: releasing non-existent buffer is not an error

**Error Codes:**
- None (always succeeds, even if buffer doesn't exist)

---

### WLBLUR_OP_RENDER_BLUR (5)

**Purpose:** Render blur effect on source buffer.

**Request Structure:**
```c
struct wlblur_render_blur_request {
    struct wlblur_request_header header;
    uint32_t source_buffer_id;  // Input texture (backdrop)
    uint32_t node_id;           // Blur node with parameters
    uint32_t n_damage_rects;    // Number of damage rectangles
    // Followed by: struct wlblur_rect damage_rects[n_damage_rects]
};

struct wlblur_rect {
    int32_t x1, y1;             // Top-left corner
    int32_t x2, y2;             // Bottom-right corner (exclusive)
};
```

**Response Structure:**
```c
struct wlblur_render_blur_response {
    struct wlblur_response_header header;
    uint32_t blurred_buffer_id; // Output texture (daemon-owned)
};
```

**Ancillary Data (Optional):**
- **Wait fence:** File descriptor for syncobj timeline to wait on before reading source
- **Signal fence:** File descriptor for syncobj timeline to signal when render completes

**Semantics:**
- Applies blur algorithm to `source_buffer_id` using parameters from `node_id`
- Damage region specifies what area needs re-rendering (optimization)
- Returns `blurred_buffer_id` which can be used as source for compositor
- Blurred buffer is daemon-owned (no need to release)
- Synchronous: daemon waits for blur completion before responding

**Damage Handling:**
- If `n_damage_rects` is 0: full-screen blur
- Compositor should pre-expand damage by blur radius before sending
- Daemon clips rendering to union of damage rectangles

**Error Codes:**
- `WLBLUR_ERROR_INVALID_BUFFER_ID`: Source buffer doesn't exist
- `WLBLUR_ERROR_INVALID_NODE`: Node ID doesn't exist
- `WLBLUR_ERROR_GL_ERROR`: OpenGL/Vulkan rendering failed
- `WLBLUR_ERROR_OUT_OF_MEMORY`: Failed to allocate output buffer

---

### WLBLUR_OP_SET_PARAMETERS (6)

**Purpose:** Update blur node parameters (strength, alpha, corner radius, etc.).

**Request Structure:**
```c
struct wlblur_set_parameters_request {
    struct wlblur_request_header header;
    uint32_t node_id;           // Blur node to update (0 = global defaults)
    float    strength;          // Blur strength multiplier (0.0 - 2.0)
    float    alpha;             // Transparency (0.0 = transparent, 1.0 = opaque)
    int32_t  corner_radius;     // Corner rounding in pixels (0 = sharp)
    uint8_t  only_blur_bottom_layer; // 1 = only blur bottom layer, 0 = all layers
    uint8_t  reserved[3];       // Padding for alignment
};
```

**Response Structure:**
```c
struct wlblur_set_parameters_response {
    struct wlblur_response_header header;
};
```

**Semantics:**
- Updates parameters for specific node or global defaults
- Changes take effect on next `WLBLUR_OP_RENDER_BLUR`
- `node_id = 0`: Sets global defaults for newly created nodes
- Parameters are clamped to valid ranges

**Parameter Ranges:**
- `strength`: [0.0, 2.0] - Clamped by daemon
- `alpha`: [0.0, 1.0] - Clamped by daemon
- `corner_radius`: [0, min(width, height) / 2] - Clamped by daemon

**Error Codes:**
- `WLBLUR_ERROR_INVALID_NODE`: Node ID doesn't exist (unless node_id == 0)

---

### WLBLUR_OP_LINK_MASK (7)

**Purpose:** Link a buffer as transparency mask (stencil) for blur node.

**Request Structure:**
```c
struct wlblur_link_mask_request {
    struct wlblur_request_header header;
    uint32_t node_id;           // Blur node
    uint32_t mask_buffer_id;    // Buffer to use as mask (0 = unlink)
};
```

**Response Structure:**
```c
struct wlblur_link_mask_response {
    struct wlblur_response_header header;
};
```

**Semantics:**
- Links `mask_buffer_id` as transparency mask for blur rendering
- During blur: mask rendered to stencil buffer, blur only applied where stencil == 1
- `mask_buffer_id = 0`: Unlinks mask (blur entire region)
- Useful for window-shaped blurs (blur only visible window region)

**Error Codes:**
- `WLBLUR_ERROR_INVALID_NODE`: Node ID doesn't exist
- `WLBLUR_ERROR_INVALID_BUFFER_ID`: Mask buffer doesn't exist

---

### WLBLUR_OP_PING (8)

**Purpose:** Health check / keep-alive.

**Request Structure:**
```c
struct wlblur_ping_request {
    struct wlblur_request_header header;
    uint64_t timestamp;         // Client timestamp (nanoseconds)
};
```

**Response Structure:**
```c
struct wlblur_ping_response {
    struct wlblur_response_header header;
    uint64_t timestamp;         // Echoed client timestamp
    uint64_t daemon_uptime;     // Daemon uptime in nanoseconds
};
```

**Semantics:**
- Verifies daemon is alive and responsive
- Can measure round-trip latency
- No side effects

**Error Codes:**
- None (always succeeds if daemon is alive)

---

### WLBLUR_OP_CLEANUP_CLIENT (9)

**Purpose:** Destroy all resources owned by this client connection.

**Request Structure:**
```c
struct wlblur_cleanup_client_request {
    struct wlblur_request_header header;
};
```

**Response Structure:**
```c
struct wlblur_cleanup_client_response {
    struct wlblur_response_header header;
    uint32_t nodes_destroyed;   // Number of nodes cleaned up
    uint32_t buffers_released;  // Number of buffers released
};
```

**Semantics:**
- Destroys all blur nodes created by this client
- Releases all buffers imported by this client
- Called automatically when client disconnects
- Can be called explicitly for manual cleanup

**Error Codes:**
- None (always succeeds)

---

## Error Codes

All error codes are signed 32-bit integers. Zero indicates success.

```c
#define WLBLUR_ERROR_NONE                  0
#define WLBLUR_ERROR_INVALID_PROTOCOL     -1  // Unsupported protocol_version
#define WLBLUR_ERROR_INVALID_OP           -2  // Unknown operation code
#define WLBLUR_ERROR_INVALID_NODE         -3  // Node ID doesn't exist
#define WLBLUR_ERROR_INVALID_BUFFER_ID    -4  // Buffer ID doesn't exist
#define WLBLUR_ERROR_DMABUF_IMPORT_FAILED -5  // EGL/GL DMA-BUF import failed
#define WLBLUR_ERROR_UNSUPPORTED_FORMAT   -6  // DRM format/modifier not supported
#define WLBLUR_ERROR_INVALID_DMABUF       -7  // Malformed DMA-BUF attributes
#define WLBLUR_ERROR_GL_ERROR             -8  // OpenGL rendering error
#define WLBLUR_ERROR_OUT_OF_MEMORY        -9  // Memory allocation failed
#define WLBLUR_ERROR_INVALID_DIMENSIONS   -10 // Width/height out of range
#define WLBLUR_ERROR_MAX_NODES_EXCEEDED   -11 // Per-client node limit reached
#define WLBLUR_ERROR_PAYLOAD_SIZE_MISMATCH -12 // Payload size doesn't match operation
```

### Error Handling

**Client behavior on error:**
1. Check `error_code` in response header
2. Log error with human-readable message
3. Fall back to non-blurred rendering
4. Optionally retry with different parameters

**Daemon behavior on error:**
1. Log error details (client PID, operation, parameters)
2. Send error response to client
3. Clean up any partially created resources
4. Continue serving other clients (errors don't crash daemon)

---

## SCM_RIGHTS Mechanism

### File Descriptor Passing

DMA-BUF file descriptors are passed via Unix domain socket ancillary data.

**Sending (Client → Daemon):**
```c
struct msghdr msg = {0};
struct cmsghdr *cmsg;
char cmsg_buf[CMSG_SPACE(sizeof(int) * n_fds)];

// Regular message (struct)
struct iovec iov = {
    .iov_base = &request,
    .iov_len = sizeof(request),
};
msg.msg_iov = &iov;
msg.msg_iovlen = 1;

// Ancillary data (file descriptors)
msg.msg_control = cmsg_buf;
msg.msg_controllen = sizeof(cmsg_buf);

cmsg = CMSG_FIRSTHDR(&msg);
cmsg->cmsg_level = SOL_SOCKET;
cmsg->cmsg_type = SCM_RIGHTS;
cmsg->cmsg_len = CMSG_LEN(sizeof(int) * n_fds);

memcpy(CMSG_DATA(cmsg), fds, sizeof(int) * n_fds);

sendmsg(socket_fd, &msg, 0);
```

**Receiving (Daemon ← Client):**
```c
struct msghdr msg = {0};
char cmsg_buf[CMSG_SPACE(sizeof(int) * 4)]; // Max 4 FDs (DMA-BUF planes)

struct iovec iov = {
    .iov_base = &request,
    .iov_len = sizeof(request),
};
msg.msg_iov = &iov;
msg.msg_iovlen = 1;

msg.msg_control = cmsg_buf;
msg.msg_controllen = sizeof(cmsg_buf);

recvmsg(client_fd, &msg, 0);

// Extract file descriptors
struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
if (cmsg && cmsg->cmsg_type == SCM_RIGHTS) {
    int *fds = (int *)CMSG_DATA(cmsg);
    int n_fds = (cmsg->cmsg_len - CMSG_LEN(0)) / sizeof(int);
}
```

### FD Ownership

- **After send:** Sender retains FD (can close or reuse)
- **After receive:** Receiver owns new FD (must close when done)
- **Import:** Daemon closes FDs after EGL import (EGLImage holds GPU resource)

---

## Message Flow Examples

### Example 1: First-Time Blur

**Scenario:** Compositor wants to blur a window for the first time.

```
Compositor                          Daemon
    │                                  │
    ├─(1) CREATE_NODE ────────────────→│
    │   parent_id: 0                   │ • Allocate node
    │   width: 1920, height: 1080      │ • node_id = 42
    │←───────────── node_id: 42 ───────┤
    │                                  │
    ├─(2) SET_PARAMETERS ──────────────→│
    │   node_id: 42                    │ • Update node params
    │   strength: 1.5, alpha: 0.95     │
    │←───────────── OK ────────────────┤
    │                                  │
    ├─(3) IMPORT_DMABUF ───────────────→│
    │   width: 1920, height: 1080      │ • Import DMA-BUF as EGLImage
    │   format: ARGB8888               │ • Create GL texture
    │   [SCM_RIGHTS: fd=10]            │ • buffer_id = 100
    │←────── buffer_id: 100 ───────────┤
    │                                  │
    ├─(4) RENDER_BLUR ─────────────────→│
    │   source_buffer_id: 100          │ • Apply Dual Kawase blur
    │   node_id: 42                    │ • Multi-pass rendering
    │   damage: [(0,0,1920,1080)]      │ • Return output texture
    │←─ blurred_buffer_id: 200 ────────┤
    │                                  │
    │ (Compositor imports buffer_id 200 as DMA-BUF and composites)
    │                                  │
    ├─(5) RELEASE_BUFFER ──────────────→│
    │   buffer_id: 100                 │ • Decrement refcount
    │                                  │ • Destroy if 0
    │                                  │
```

### Example 2: Cached Blur (Subsequent Frames)

**Scenario:** Window hasn't moved, only partial damage.

```
Compositor                          Daemon
    │                                  │
    ├─(1) IMPORT_DMABUF ───────────────→│
    │   [New backdrop frame]           │ • Import new frame
    │←────── buffer_id: 101 ───────────┤
    │                                  │
    ├─(2) RENDER_BLUR ─────────────────→│
    │   source_buffer_id: 101          │ • Optimized render
    │   node_id: 42 (cached)           │ • Only damage region
    │   damage: [(100,100,300,300)]    │ • Reuse shader state
    │←─ blurred_buffer_id: 201 ────────┤
    │                                  │
    ├─(3) RELEASE_BUFFER ──────────────→│
    │   buffer_id: 101                 │
    │                                  │
```

### Example 3: Window Destruction

**Scenario:** Window closed, clean up blur resources.

```
Compositor                          Daemon
    │                                  │
    ├─(1) DESTROY_NODE ────────────────→│
    │   node_id: 42                    │ • Destroy node
    │←───────────── OK ────────────────┤ • Unlink mask
    │                                  │ • Free resources
```

### Example 4: Daemon Crash Recovery

**Scenario:** Daemon crashes and restarts mid-session.

```
Compositor                          Daemon
    │                                  │
    ├─(1) RENDER_BLUR ────────────X  (crash)
    │   (connection broken)            │
    │                                  │
    │ • Detect disconnect              │
    │ • Restart daemon                 │ (restart)
    │ • Wait for socket                │
    │                                  │
    ├─(2) CREATE_NODE ────────────────→│ • Recreate node_id: 42
    │←────── node_id: 42 ──────────────┤
    │                                  │
    ├─(3) SET_PARAMETERS ──────────────→│ • Restore parameters
    │←───────────── OK ────────────────┤
    │                                  │
    │ (Resume normal operation)         │
```

---

## Versioning Strategy

### Protocol Version Field

Every request includes `protocol_version` in the header. This allows future backwards-incompatible changes.

**Current version:** `1`

**Version negotiation:**
1. Client sends request with `protocol_version = 1`
2. Daemon checks version:
   - If supported: process request normally
   - If unsupported: respond with `WLBLUR_ERROR_INVALID_PROTOCOL`
3. Client can retry with different version or fall back to non-blur rendering

### Future Compatibility

**Minor changes (backwards-compatible):**
- New operation codes (add to end of enum)
- New optional fields in existing structs (append to end)
- New error codes

**Major changes (breaking):**
- Changing field sizes/types in existing structs
- Removing operations
- Changing operation semantics

**Major changes require `protocol_version` increment.**

### Feature Detection

Clients can detect daemon capabilities via:
- **Ping operation:** Check daemon responds (indicates basic support)
- **Error codes:** Try operation, check if `WLBLUR_ERROR_INVALID_OP` returned
- **Future:** Add `WLBLUR_OP_GET_CAPABILITIES` operation

---

## Security Considerations

### Socket Security

**File permissions:**
- Socket: `0700` (only owner can connect)
- Socket directory: `/run/user/$UID/` (user-private tmpfs)

**UID validation:**
- Daemon must verify connecting client has same UID
- Use `SO_PEERCRED` to get client PID/UID/GID:

```c
struct ucred cred;
socklen_t len = sizeof(cred);
getsockopt(client_fd, SOL_SOCKET, SO_PEERCRED, &cred, &len);

if (cred.uid != getuid()) {
    close(client_fd);
    return;
}
```

### Parameter Validation

**All input must be validated:**

```c
// Width/height
if (width <= 0 || width > MAX_TEXTURE_SIZE) return WLBLUR_ERROR_INVALID_DIMENSIONS;
if (height <= 0 || height > MAX_TEXTURE_SIZE) return WLBLUR_ERROR_INVALID_DIMENSIONS;

// Strength/alpha
strength = clamp(strength, 0.0f, 2.0f);
alpha = clamp(alpha, 0.0f, 1.0f);

// Corner radius
corner_radius = clamp(corner_radius, 0, min(width, height) / 2);

// DMA-BUF planes
if (n_planes == 0 || n_planes > 4) return WLBLUR_ERROR_INVALID_DMABUF;

// Operation code
if (op >= WLBLUR_OP_MAX) return WLBLUR_ERROR_INVALID_OP;

// Payload size
if (header.payload_size != expected_size) return WLBLUR_ERROR_PAYLOAD_SIZE_MISMATCH;
```

### Resource Limits

**Per-client limits:**
- **Max blur nodes:** 100 (prevents memory exhaustion)
- **Max imported buffers:** 1000 (prevents FD exhaustion)
- **Max request size:** 1 MB (prevents DoS)
- **Max damage rectangles:** 256 (prevents CPU exhaustion)

**Enforcement:**
```c
struct client_state {
    uint32_t node_count;
    uint32_t buffer_count;
};

if (client->node_count >= MAX_NODES_PER_CLIENT) {
    return WLBLUR_ERROR_MAX_NODES_EXCEEDED;
}
```

### DMA-BUF Security

**Validation:**
- Verify FDs are valid DMA-BUF descriptors
- Check format/modifier supported by GPU
- Validate dimensions against GPU limits
- Prevent integer overflow in size calculations

**Import safeguards:**
```c
// Check format supported
if (!is_format_supported(format, modifier)) {
    return WLBLUR_ERROR_UNSUPPORTED_FORMAT;
}

// Prevent integer overflow
uint64_t total_size = (uint64_t)stride * height;
if (total_size > MAX_BUFFER_SIZE) {
    return WLBLUR_ERROR_INVALID_DMABUF;
}
```

### GL Context Isolation

**Per-client contexts (optional):**
- Each client gets dedicated GL context
- Prevents cross-client state pollution
- Limits blast radius of GL errors

**Single-context mode (simpler):**
- Save/restore GL state between clients
- Validate all GL state after client operations
- Use `glGetError()` to detect corruption

### Crash Isolation

**Daemon crash doesn't affect compositor:**
- Compositor continues running without blur
- Compositor can restart daemon and restore state
- Graceful degradation (blur disabled, not crash)

**Compositor crash doesn't leak daemon resources:**
- Daemon detects client disconnect (EPIPE, ECONNRESET)
- Automatic cleanup via `WLBLUR_OP_CLEANUP_CLIENT` logic
- All client resources freed

---

## Performance Considerations

### Latency Budget

**IPC overhead:**
- Unix socket sendmsg/recvmsg: ~0.05ms each
- Context switch: ~0.01ms
- Total round-trip: ~0.16ms

**Blur rendering:**
- DMA-BUF import: ~0.1ms (one-time per buffer)
- Dual Kawase 3-pass blur: ~1.2ms
- Total: ~1.4ms

**60 FPS target:**
- Frame budget: 16.6ms
- Blur budget: ~1.6ms (acceptable, ~10% of frame)

### Optimization Strategies

**1. Async pipeline:**
```
Frame N:   Start blur → Continue rendering other content
Frame N+1: Composite blurred result from frame N
```
- Hides IPC latency
- Increases throughput

**2. Damage tracking:**
- Compositor expands damage by blur radius before sending
- Daemon only renders damaged region
- Skip blur if damage empty

**3. Caching:**
- Daemon caches blur node parameters
- Reuse GL state (shaders, FBOs) between frames
- Only recompile shaders when parameters change

**4. Batch operations:**
- Send multiple `IMPORT_DMABUF` + `RENDER_BLUR` in single message
- Reduces syscall overhead

---

## Implementation Notes

### Recommended Client Flow

```c
// Startup
int daemon_fd = connect_to_daemon("/run/user/1000/wlblur.sock");
if (daemon_fd < 0) {
    // Daemon not running, disable blur
}

// Per-window initialization
uint32_t node_id = create_blur_node(daemon_fd, 0, width, height);
set_blur_parameters(daemon_fd, node_id, strength, alpha, radius);

// Per-frame rendering
uint32_t backdrop_buffer_id = import_dmabuf(daemon_fd, backdrop_dmabuf);
uint32_t blurred_buffer_id = render_blur(daemon_fd, backdrop_buffer_id, node_id, damage);
composite_to_screen(blurred_buffer_id);
release_buffer(daemon_fd, backdrop_buffer_id);

// Cleanup
destroy_blur_node(daemon_fd, node_id);
close(daemon_fd);
```

### Recommended Daemon Architecture

**Threading model:**
- Main thread: Accept connections, dispatch to workers
- Per-client thread: Dedicated GL context, process requests
- OR: Single GL thread with async request queue (simpler)

**GL context management:**
```c
// Per-client (complex, high performance)
EGLContext client_ctx = eglCreateContext(...);
eglMakeCurrent(..., client_ctx);

// Single context (simple, good performance)
eglMakeCurrent(..., shared_ctx);
// Save/restore GL state between clients
```

**Resource tracking:**
```c
struct daemon {
    hash_table_t *nodes;      // node_id → struct blur_node
    hash_table_t *buffers;    // buffer_id → struct buffer
    hash_table_t *clients;    // client_fd → struct client_state
};
```

---

## References

- **DMA-BUF:** Linux kernel documentation on DMA-BUF framework
- **EGL_EXT_image_dma_buf_import:** EGL extension for DMA-BUF import
- **SCM_RIGHTS:** `unix(7)` man page, ancillary data section
- **wlroots DMA-BUF APIs:** `wlr_buffer_get_dmabuf()`, `wlr_buffer_from_dmabuf()`

**Related wlblur documentation:**
- `docs/investigation/scenefx-investigation/daemon-translation.md` - Detailed operation translation examples
- `docs/post-investigation/blur-daemon-approach.md` - Architecture rationale
- `docs/post-investigation/addendum-why-ipc-is-better.md` - Performance analysis

---

## Appendix: Complete Header File

```c
/* wlblur IPC Protocol v1 */

#ifndef WLBLUR_IPC_H
#define WLBLUR_IPC_H

#include <stdint.h>

/* Protocol version */
#define WLBLUR_PROTOCOL_VERSION 1

/* Operation codes */
#define WLBLUR_OP_CREATE_NODE     1
#define WLBLUR_OP_DESTROY_NODE    2
#define WLBLUR_OP_IMPORT_DMABUF   3
#define WLBLUR_OP_RELEASE_BUFFER  4
#define WLBLUR_OP_RENDER_BLUR     5
#define WLBLUR_OP_SET_PARAMETERS  6
#define WLBLUR_OP_LINK_MASK       7
#define WLBLUR_OP_PING            8
#define WLBLUR_OP_CLEANUP_CLIENT  9
#define WLBLUR_OP_MAX             10

/* Error codes */
#define WLBLUR_ERROR_NONE                  0
#define WLBLUR_ERROR_INVALID_PROTOCOL     -1
#define WLBLUR_ERROR_INVALID_OP           -2
#define WLBLUR_ERROR_INVALID_NODE         -3
#define WLBLUR_ERROR_INVALID_BUFFER_ID    -4
#define WLBLUR_ERROR_DMABUF_IMPORT_FAILED -5
#define WLBLUR_ERROR_UNSUPPORTED_FORMAT   -6
#define WLBLUR_ERROR_INVALID_DMABUF       -7
#define WLBLUR_ERROR_GL_ERROR             -8
#define WLBLUR_ERROR_OUT_OF_MEMORY        -9
#define WLBLUR_ERROR_INVALID_DIMENSIONS   -10
#define WLBLUR_ERROR_MAX_NODES_EXCEEDED   -11
#define WLBLUR_ERROR_PAYLOAD_SIZE_MISMATCH -12

/* Request header (all requests) */
struct wlblur_request_header {
    uint32_t protocol_version;
    uint32_t op;
    uint32_t request_id;
    uint32_t payload_size;
};

/* Response header (all responses) */
struct wlblur_response_header {
    uint32_t request_id;
    int32_t  error_code;
    uint32_t payload_size;
};

/* CREATE_NODE */
struct wlblur_create_node_request {
    struct wlblur_request_header header;
    uint32_t parent_id;
    int32_t  width;
    int32_t  height;
};

struct wlblur_create_node_response {
    struct wlblur_response_header header;
    uint32_t node_id;
};

/* DESTROY_NODE */
struct wlblur_destroy_node_request {
    struct wlblur_request_header header;
    uint32_t node_id;
};

struct wlblur_destroy_node_response {
    struct wlblur_response_header header;
};

/* IMPORT_DMABUF */
struct wlblur_import_dmabuf_request {
    struct wlblur_request_header header;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint8_t  n_planes;
    uint8_t  reserved[3];
    uint32_t offsets[4];
    uint32_t strides[4];
    uint64_t modifier;
};

struct wlblur_import_dmabuf_response {
    struct wlblur_response_header header;
    uint32_t buffer_id;
};

/* RELEASE_BUFFER */
struct wlblur_release_buffer_request {
    struct wlblur_request_header header;
    uint32_t buffer_id;
};

struct wlblur_release_buffer_response {
    struct wlblur_response_header header;
};

/* RENDER_BLUR */
struct wlblur_rect {
    int32_t x1, y1;
    int32_t x2, y2;
};

struct wlblur_render_blur_request {
    struct wlblur_request_header header;
    uint32_t source_buffer_id;
    uint32_t node_id;
    uint32_t n_damage_rects;
    /* Followed by: struct wlblur_rect damage_rects[n_damage_rects] */
};

struct wlblur_render_blur_response {
    struct wlblur_response_header header;
    uint32_t blurred_buffer_id;
};

/* SET_PARAMETERS */
struct wlblur_set_parameters_request {
    struct wlblur_request_header header;
    uint32_t node_id;
    float    strength;
    float    alpha;
    int32_t  corner_radius;
    uint8_t  only_blur_bottom_layer;
    uint8_t  reserved[3];
};

struct wlblur_set_parameters_response {
    struct wlblur_response_header header;
};

/* LINK_MASK */
struct wlblur_link_mask_request {
    struct wlblur_request_header header;
    uint32_t node_id;
    uint32_t mask_buffer_id;
};

struct wlblur_link_mask_response {
    struct wlblur_response_header header;
};

/* PING */
struct wlblur_ping_request {
    struct wlblur_request_header header;
    uint64_t timestamp;
};

struct wlblur_ping_response {
    struct wlblur_response_header header;
    uint64_t timestamp;
    uint64_t daemon_uptime;
};

/* CLEANUP_CLIENT */
struct wlblur_cleanup_client_request {
    struct wlblur_request_header header;
};

struct wlblur_cleanup_client_response {
    struct wlblur_response_header header;
    uint32_t nodes_destroyed;
    uint32_t buffers_released;
};

#endif /* WLBLUR_IPC_H */
```

---

**End of Specification**
