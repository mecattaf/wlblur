# Compositor Integration Patterns

**Document Version:** 1.0
**Last Updated:** 2025-01-15
**Status:** Design Complete, Implementation Pending

---

## Overview

This document provides concrete patterns for integrating wlblur daemon into Wayland compositors. The integration is intentionally minimal (~200 lines) and follows a standard workflow regardless of compositor architecture.

**Supported Compositors:**
- **scroll:** wlroots-based, C (~220 lines)
- **niri:** Smithay-based, Rust (~180 lines)
- **Sway/River:** Similar to scroll pattern
- **Custom compositors:** Generic pattern applicable

---

## Integration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Compositor Process                       │
│                                                             │
│  ┌────────────────────────────────────────────────────┐   │
│  │  Standard Compositor Rendering                     │   │
│  │  (unchanged)                                       │   │
│  └────────────────────────────────────────────────────┘   │
│                          │                                  │
│                          ↓                                  │
│  ┌────────────────────────────────────────────────────┐   │
│  │  Blur Integration Layer (~200 lines)               │   │
│  │                                                     │   │
│  │  1. Detect blur-eligible windows                   │   │
│  │     • Config-based: blur_enabled                   │   │
│  │     • Protocol-based: future Wayland extension     │   │
│  │                                                     │   │
│  │  2. Export backdrop texture                        │   │
│  │     • Render content behind window to FBO          │   │
│  │     • Export as DMA-BUF (wlr_buffer_get_dmabuf)   │   │
│  │                                                     │   │
│  │  3. Send blur request                              │   │
│  │     • Unix socket: SOCK_SEQPACKET                  │   │
│  │     • FD passing: SCM_RIGHTS                       │   │
│  │                                                     │   │
│  │  4. Import blurred result                          │   │
│  │     • Receive DMA-BUF FD                           │   │
│  │     • Import as wlr_buffer or GL texture           │   │
│  │                                                     │   │
│  │  5. Composite into scene graph                     │   │
│  │     • Place blurred backdrop behind window         │   │
│  │     • Standard wlr_scene_buffer or custom node     │   │
│  └────────────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────────────┘
                          │
                          │ IPC (Unix socket + DMA-BUF)
                          ↓
                    wlblurd Daemon
```

---

## Generic Integration Pattern

### Phase 1: Detect Blur Windows

```c
bool should_blur_window(struct compositor_window* window) {
    // Method 1: Configuration-based
    if (config->blur_enabled && window_is_transparent(window)) {
        return true;
    }

    // Method 2: Protocol-based (future)
    // if (window->blur_protocol_enabled) {
    //     return true;
    // }

    return false;
}
```

### Phase 2: Export Backdrop

```c
struct wlr_buffer* export_backdrop(struct compositor_window* window) {
    // 1. Create offscreen FBO
    struct wlr_fbo* fbo = wlr_fbo_create(window->width, window->height);

    // 2. Render everything BEHIND this window
    wlr_renderer_begin(renderer, fbo->texture);

    // Render desktop background
    render_wallpaper();

    // Render windows below this one
    for (each window below) {
        render_window(window);
    }

    wlr_renderer_end(renderer);

    // 3. Convert FBO to wlr_buffer
    struct wlr_buffer* buffer = wlr_fbo_to_buffer(fbo);

    return buffer;
}
```

### Phase 3: Request Blur

```c
int request_blur(struct blur_client* client, struct wlr_buffer* backdrop,
                 uint32_t blur_id, struct blur_params* params) {
    // 1. Export backdrop as DMA-BUF
    struct wlr_dmabuf_attributes dmabuf;
    if (!wlr_buffer_get_dmabuf(backdrop, &dmabuf)) {
        return -1;
    }

    // 2. Build request
    struct blur_render_request req = {
        .header = {
            .magic = 0x424C5552,
            .version = 1,
            .client_id = client->client_id,
            .sequence = client->sequence++,
            .opcode = BLUR_OP_RENDER,
            .payload_size = sizeof(req) - sizeof(req.header),
        },
        .blur_id = blur_id,
        .input_buffer_id = 0,  // Import on first use
        .params = *params,
    };

    // 3. Send request + FD
    send_request_with_fd(client->sockfd, &req, sizeof(req), dmabuf.fd[0]);

    // 4. Receive response + blurred FD
    struct blur_render_response resp;
    int blurred_fd = recv_response_with_fd(client->sockfd, &resp, sizeof(resp));

    return blurred_fd;
}
```

### Phase 4: Composite Result

```c
void composite_blurred_window(struct compositor_window* window, int blurred_fd) {
    // 1. Import blurred DMA-BUF
    struct wlr_dmabuf_attributes dmabuf = {
        .width = window->width,
        .height = window->height,
        .format = DRM_FORMAT_ARGB8888,
        .fd[0] = blurred_fd,
        // ... fill other fields from daemon response
    };

    struct wlr_buffer* blurred_buffer = wlr_buffer_from_dmabuf(&dmabuf);

    // 2. Add to scene graph
    struct wlr_scene_buffer* blur_layer =
        wlr_scene_buffer_create(window->scene_tree, blurred_buffer);

    // 3. Position behind window
    wlr_scene_node_place_below(&blur_layer->node, &window->surface_node->node);

    // 4. Render final frame
    // (compositor's normal rendering continues)
}
```

---

## scroll Integration (wlroots)

### File Structure

```
scroll/
├── sway/desktop/blur_integration.c    # NEW (~200 lines)
├── sway/desktop/blur_integration.h    # NEW (~30 lines)
├── sway/desktop/render.c              # MODIFIED (~20 lines added)
├── sway/config.c                      # MODIFIED (~10 lines for config)
├── sway/meson.build                   # MODIFIED (~2 lines)
└── include/sway/config.h              # MODIFIED (~5 lines)
```

### blur_integration.h

```c
// sway/desktop/blur_integration.h

#ifndef SWAY_BLUR_INTEGRATION_H
#define SWAY_BLUR_INTEGRATION_H

#include <stdint.h>
#include <stdbool.h>
#include <wlr/types/wlr_buffer.h>

struct blur_client;
struct sway_container;

/**
 * Initialize blur daemon connection
 * Returns NULL if daemon not available
 */
struct blur_client* blur_client_init(void);

/**
 * Destroy blur client and cleanup
 */
void blur_client_destroy(struct blur_client* client);

/**
 * Create blur node for container
 * Returns blur_id, or 0 on error
 */
uint32_t blur_create_node(struct blur_client* client,
                           int width, int height);

/**
 * Request blur for container backdrop
 * Returns blurred buffer, or NULL on error
 */
struct wlr_buffer* blur_request(struct blur_client* client,
                                 uint32_t blur_id,
                                 struct wlr_buffer* backdrop);

/**
 * Destroy blur node
 */
void blur_destroy_node(struct blur_client* client, uint32_t blur_id);

#endif
```

### blur_integration.c (Excerpt)

```c
// sway/desktop/blur_integration.c

#include "sway/desktop/blur_integration.h"
#include "sway/config.h"
#include <sys/socket.h>
#include <sys/un.h>
#include <wlr/types/wlr_buffer.h>

struct blur_client {
    int sockfd;
    uint32_t client_id;
    uint32_t sequence;
};

struct blur_client* blur_client_init(void) {
    const char* socket_path = getenv("WLBLUR_SOCKET");
    if (!socket_path) {
        char path[256];
        snprintf(path, sizeof(path), "/run/user/%d/wlblur.sock", getuid());
        socket_path = path;
    }

    // Connect to daemon
    int sockfd = socket(AF_UNIX, SOCK_SEQPACKET, 0);
    if (sockfd < 0) {
        sway_log(SWAY_INFO, "Failed to create socket for blur daemon");
        return NULL;
    }

    struct sockaddr_un addr = {
        .sun_family = AF_UNIX,
    };
    strncpy(addr.sun_path, socket_path, sizeof(addr.sun_path) - 1);

    if (connect(sockfd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        sway_log(SWAY_INFO, "Blur daemon not available at %s", socket_path);
        close(sockfd);
        return NULL;
    }

    // Handshake
    struct blur_handshake_request req = {
        .header = {
            .magic = 0x424C5552,
            .version = 1,
            .client_id = 0,
            .sequence = 0,
            .opcode = BLUR_OP_HANDSHAKE,
            .payload_size = sizeof(req) - sizeof(req.header),
        },
    };
    strncpy(req.compositor_name, "scroll", sizeof(req.compositor_name));

    send(sockfd, &req, sizeof(req), 0);

    struct blur_handshake_response resp;
    recv(sockfd, &resp, sizeof(resp), 0);

    if (resp.header.status != 0) {
        sway_log(SWAY_ERROR, "Blur daemon handshake failed");
        close(sockfd);
        return NULL;
    }

    struct blur_client* client = calloc(1, sizeof(*client));
    client->sockfd = sockfd;
    client->client_id = resp.client_id;
    client->sequence = 1;

    sway_log(SWAY_INFO, "Connected to blur daemon (client_id=%u)", client->client_id);
    return client;
}

struct wlr_buffer* blur_request(struct blur_client* client,
                                 uint32_t blur_id,
                                 struct wlr_buffer* backdrop) {
    // Export backdrop as DMA-BUF
    struct wlr_dmabuf_attributes dmabuf;
    if (!wlr_buffer_get_dmabuf(backdrop, &dmabuf)) {
        sway_log(SWAY_ERROR, "Failed to export backdrop as DMA-BUF");
        return NULL;
    }

    // Build render request
    struct blur_render_request req = {
        .header = {
            .magic = 0x424C5552,
            .version = 1,
            .client_id = client->client_id,
            .sequence = client->sequence++,
            .opcode = BLUR_OP_RENDER,
            .payload_size = sizeof(req) - sizeof(req.header),
        },
        .blur_id = blur_id,
        .params = {
            .algorithm = WLBLUR_ALGORITHM_KAWASE,
            .passes = 2,
            .offset = 1.25,
            .vibrancy = 0.2,
            .brightness = 0.0,
            .contrast = 1.0,
            .noise = 0.01,
        },
    };

    // Send request with DMA-BUF FD
    send_dmabuf_fd(client->sockfd, &req, sizeof(req), dmabuf.fd[0]);

    // Receive response
    struct blur_render_response resp;
    int blurred_fd = recv_dmabuf_fd(client->sockfd, &resp, sizeof(resp));

    if (resp.header.status != 0 || blurred_fd < 0) {
        sway_log(SWAY_ERROR, "Blur request failed (status=%d)", resp.header.status);
        return NULL;
    }

    // Import blurred result
    struct wlr_dmabuf_attributes blurred_dmabuf = dmabuf;  // Copy attributes
    blurred_dmabuf.fd[0] = blurred_fd;

    struct wlr_buffer* blurred_buffer = wlr_buffer_from_dmabuf(&blurred_dmabuf);
    return blurred_buffer;
}

// ... (send_dmabuf_fd, recv_dmabuf_fd implementations)
```

### render.c Integration

```c
// sway/desktop/render.c (modifications)

#include "sway/desktop/blur_integration.h"

// Global blur client (initialized in main)
extern struct blur_client* global_blur_client;

void render_container(struct sway_output* output,
                      struct sway_container* con,
                      struct render_data* data) {
    // Existing decoration/shadow rendering
    if (con->decoration.shadow) {
        render_shadow(output, con);
    }

    // NEW: Blur support
    if (config->blur_enabled && con->blur_id != 0) {
        // Render backdrop (everything behind window)
        struct wlr_fbo* backdrop_fbo = wlr_fbo_create(con->width, con->height);
        wlr_renderer_begin(output->wlr_output->renderer, backdrop_fbo->texture);

        // Render content below this container
        render_workspace_below_container(output, con);

        wlr_renderer_end(output->wlr_output->renderer);

        struct wlr_buffer* backdrop = wlr_fbo_to_buffer(backdrop_fbo);

        // Request blur from daemon
        struct wlr_buffer* blurred = blur_request(global_blur_client,
                                                   con->blur_id,
                                                   backdrop);

        if (blurred) {
            // Add blurred layer to scene graph
            struct wlr_scene_buffer* blur_layer =
                wlr_scene_buffer_create(con->scene_tree, blurred);

            wlr_scene_node_place_below(&blur_layer->node,
                                      &con->view->scene_tree->node);

            wlr_buffer_drop(blurred);
        }

        wlr_buffer_drop(backdrop);
    }

    // Existing window rendering
    render_view(output, con->view, data);
}
```

### Config Integration

```c
// sway/config.c

#include "sway/desktop/blur_integration.h"

static struct cmd_results* cmd_blur_enable(int argc, char** argv) {
    config->blur_enabled = parse_boolean(argv[0], config->blur_enabled);

    // Initialize blur client on first enable
    if (config->blur_enabled && !global_blur_client) {
        global_blur_client = blur_client_init();
        if (!global_blur_client) {
            return cmd_results_new(CMD_FAILURE,
                "Failed to connect to blur daemon");
        }
    }

    return cmd_results_new(CMD_SUCCESS, NULL);
}
```

**Total scroll Integration:** ~220 lines across 5 files

---

## niri Integration (Rust/Smithay)

### File Structure

```
niri/
├── src/render/blur_client.rs     # NEW (~180 lines)
├── src/render/mod.rs              # MODIFIED (import blur_client)
├── src/render/renderer.rs         # MODIFIED (~15 lines)
└── Cargo.toml                     # MODIFIED (add dependencies)
```

### blur_client.rs

```rust
// src/render/blur_client.rs

use std::os::unix::net::UnixStream;
use std::os::unix::io::{AsRawFd, RawFd};
use smithay::backend::allocator::dmabuf::Dmabuf;

pub struct BlurClient {
    stream: UnixStream,
    client_id: u32,
    sequence: u32,
}

#[repr(C, packed)]
struct BlurRequestHeader {
    magic: u32,
    version: u32,
    client_id: u32,
    sequence: u32,
    opcode: u32,
    payload_size: u32,
}

#[repr(C, packed)]
struct BlurRenderRequest {
    header: BlurRequestHeader,
    blur_id: u32,
    input_buffer_id: u32,
    passes: u32,
    offset: f32,
    vibrancy: f32,
    brightness: f32,
    contrast: f32,
    noise: f32,
}

impl BlurClient {
    pub fn connect() -> Result<Self, std::io::Error> {
        let socket_path = std::env::var("WLBLUR_SOCKET")
            .unwrap_or_else(|_| {
                format!("/run/user/{}/wlblur.sock", unsafe { libc::getuid() })
            });

        let stream = UnixStream::connect(&socket_path)?;

        // Handshake
        let handshake_req = BlurRequestHeader {
            magic: 0x424C5552,
            version: 1,
            client_id: 0,
            sequence: 0,
            opcode: 0, // BLUR_OP_HANDSHAKE
            payload_size: 32,
        };

        // Send handshake
        send_struct(&stream, &handshake_req)?;

        // Receive response
        let mut resp_buf = [0u8; 32];
        stream.read_exact(&mut resp_buf)?;

        let client_id = u32::from_le_bytes([
            resp_buf[12], resp_buf[13], resp_buf[14], resp_buf[15]
        ]);

        Ok(BlurClient {
            stream,
            client_id,
            sequence: 1,
        })
    }

    pub fn create_node(&mut self, width: i32, height: i32) -> Result<u32, std::io::Error> {
        // Build CREATE_NODE request
        let req = BlurRequestHeader {
            magic: 0x424C5552,
            version: 1,
            client_id: self.client_id,
            sequence: self.sequence,
            opcode: 1, // BLUR_OP_CREATE_NODE
            payload_size: 8,
        };

        self.sequence += 1;

        // Send request
        send_struct(&self.stream, &req)?;
        self.stream.write_all(&width.to_le_bytes())?;
        self.stream.write_all(&height.to_le_bytes())?;

        // Receive response
        let mut resp_buf = [0u8; 24];
        self.stream.read_exact(&mut resp_buf)?;

        let blur_id = u32::from_le_bytes([
            resp_buf[20], resp_buf[21], resp_buf[22], resp_buf[23]
        ]);

        Ok(blur_id)
    }

    pub fn render_blur(&mut self, blur_id: u32, backdrop: &Dmabuf)
        -> Result<Dmabuf, std::io::Error> {

        // Build RENDER request
        let req = BlurRenderRequest {
            header: BlurRequestHeader {
                magic: 0x424C5552,
                version: 1,
                client_id: self.client_id,
                sequence: self.sequence,
                opcode: 5, // BLUR_OP_RENDER
                payload_size: std::mem::size_of::<BlurRenderRequest>() as u32 - 24,
            },
            blur_id,
            input_buffer_id: 0,
            passes: 2,
            offset: 1.25,
            vibrancy: 0.2,
            brightness: 0.0,
            contrast: 1.0,
            noise: 0.01,
        };

        self.sequence += 1;

        // Send request with DMA-BUF FD
        send_struct_with_fd(&self.stream, &req, backdrop.as_raw_fd())?;

        // Receive response with blurred FD
        let mut resp_buf = [0u8; 32];
        let blurred_fd = recv_with_fd(&self.stream, &mut resp_buf)?;

        // Construct Dmabuf from FD
        // (copy attributes from backdrop, update FD)
        let blurred_dmabuf = Dmabuf::builder(
            backdrop.width(),
            backdrop.height(),
            backdrop.format().code,
            backdrop.format().modifier,
        )
        .add_plane(blurred_fd, 0, backdrop.strides()[0])
        .build()
        .unwrap();

        Ok(blurred_dmabuf)
    }
}

// Helper functions for FD passing
fn send_struct_with_fd<T>(stream: &UnixStream, data: &T, fd: RawFd)
    -> Result<(), std::io::Error> {
    use nix::sys::socket::{sendmsg, MsgFlags, ControlMessage};
    use std::io::IoSlice;

    let iov = [IoSlice::new(unsafe {
        std::slice::from_raw_parts(
            data as *const T as *const u8,
            std::mem::size_of::<T>(),
        )
    })];

    let fds = [fd];
    let cmsg = [ControlMessage::ScmRights(&fds)];

    sendmsg::<()>(
        stream.as_raw_fd(),
        &iov,
        &cmsg,
        MsgFlags::empty(),
        None,
    )?;

    Ok(())
}

fn recv_with_fd(stream: &UnixStream, buf: &mut [u8]) -> Result<RawFd, std::io::Error> {
    use nix::sys::socket::{recvmsg, MsgFlags, ControlMessageOwned};
    use std::io::IoSliceMut;

    let mut iov = [IoSliceMut::new(buf)];
    let mut cmsg_buf = nix::cmsg_space!([RawFd; 1]);

    let msg = recvmsg::<()>(
        stream.as_raw_fd(),
        &mut iov,
        Some(&mut cmsg_buf),
        MsgFlags::empty(),
    )?;

    // Extract FD from control message
    for cmsg in msg.cmsgs() {
        if let ControlMessageOwned::ScmRights(fds) = cmsg {
            return Ok(fds[0]);
        }
    }

    Err(std::io::Error::new(
        std::io::ErrorKind::Other,
        "No FD received",
    ))
}
```

### Renderer Integration

```rust
// src/render/renderer.rs

use crate::render::blur_client::BlurClient;

pub struct NiriRenderer {
    blur_client: Option<BlurClient>,
    // ... existing fields
}

impl NiriRenderer {
    pub fn render_window(&mut self, window: &Window, output: &Output) {
        // Existing rendering...

        // Check if blur enabled
        if self.config.blur_enabled && window.is_transparent() {
            // Render backdrop
            let backdrop = self.render_backdrop_for_window(window);

            // Request blur
            if let Some(ref mut blur_client) = self.blur_client {
                let blur_id = window.blur_id.get_or_insert_with(|| {
                    blur_client.create_node(window.width, window.height).unwrap()
                });

                if let Ok(blurred) = blur_client.render_blur(*blur_id, &backdrop) {
                    // Composite blurred backdrop
                    self.composite_dmabuf(&blurred, window.geometry);
                }
            }
        }

        // Existing window rendering...
    }
}
```

**Total niri Integration:** ~180 lines across 3 files

---

## Common Patterns

### 1. FD Passing (Unix Socket)

```c
// Send DMA-BUF FD via SCM_RIGHTS
int send_dmabuf_fd(int sockfd, const void* data, size_t size, int fd) {
    struct msghdr msg = {0};
    struct iovec iov[1];

    iov[0].iov_base = (void*)data;
    iov[0].iov_len = size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    // Control message for FD
    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));

    return sendmsg(sockfd, &msg, 0);
}

// Receive DMA-BUF FD
int recv_dmabuf_fd(int sockfd, void* data, size_t size) {
    struct msghdr msg = {0};
    struct iovec iov[1];

    iov[0].iov_base = data;
    iov[0].iov_len = size;
    msg.msg_iov = iov;
    msg.msg_iovlen = 1;

    char cmsg_buf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    recvmsg(sockfd, &msg, 0);

    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg && cmsg->cmsg_type == SCM_RIGHTS) {
        int fd;
        memcpy(&fd, CMSG_DATA(cmsg), sizeof(int));
        return fd;
    }

    return -1;
}
```

### 2. Backdrop Rendering

```c
struct wlr_buffer* render_backdrop_for_window(struct compositor_window* window,
                                               struct compositor_output* output) {
    // Create offscreen FBO matching window size
    struct wlr_fbo* fbo = wlr_fbo_create(window->width, window->height);

    wlr_renderer_begin(output->renderer, fbo->texture);

    // Set viewport to match window
    wlr_renderer_scissor(output->renderer, &window->geometry);

    // Render wallpaper (clipped to window region)
    render_wallpaper(output);

    // Render windows below this one
    wl_list_for_each(other_window, &output->windows, link) {
        if (window_is_below(other_window, window)) {
            render_window_surface(other_window);
        }
    }

    wlr_renderer_end(output->renderer);

    // Convert to wlr_buffer
    struct wlr_buffer* buffer = wlr_fbo_to_buffer(fbo);
    wlr_fbo_destroy(fbo);

    return buffer;
}
```

### 3. Error Handling

```c
struct wlr_buffer* safe_blur_request(struct blur_client* client,
                                      uint32_t blur_id,
                                      struct wlr_buffer* backdrop) {
    struct wlr_buffer* blurred = blur_request(client, blur_id, backdrop);

    if (!blurred) {
        // Fallback: Render without blur
        sway_log(SWAY_WARN, "Blur request failed, rendering without blur");
        return wlr_buffer_lock(backdrop);  // Use unblurred backdrop
    }

    return blurred;
}
```

### 4. Resource Cleanup

```c
void destroy_window_blur(struct compositor_window* window,
                         struct blur_client* client) {
    if (window->blur_id != 0) {
        blur_destroy_node(client, window->blur_id);
        window->blur_id = 0;
    }

    if (window->blur_layer) {
        wlr_scene_node_destroy(&window->blur_layer->node);
        window->blur_layer = NULL;
    }
}
```

---

## Performance Optimization

### 1. Blur Caching (Compositor-Side)

```c
struct window_blur_cache {
    uint32_t blur_id;
    struct wlr_buffer* cached_blur;
    struct wlr_box last_geometry;
    bool dirty;
};

struct wlr_buffer* get_or_render_blur(struct compositor_window* window) {
    struct window_blur_cache* cache = &window->blur_cache;

    // Check if cache valid
    if (!cache->dirty &&
        geometry_equals(&cache->last_geometry, &window->geometry)) {
        return wlr_buffer_lock(cache->cached_blur);
    }

    // Render new blur
    struct wlr_buffer* backdrop = render_backdrop_for_window(window);
    struct wlr_buffer* blurred = blur_request(client, cache->blur_id, backdrop);

    // Update cache
    if (cache->cached_blur) {
        wlr_buffer_drop(cache->cached_blur);
    }
    cache->cached_blur = wlr_buffer_lock(blurred);
    cache->last_geometry = window->geometry;
    cache->dirty = false;

    wlr_buffer_drop(backdrop);
    return blurred;
}

// Invalidate cache on window move/resize
void on_window_geometry_changed(struct compositor_window* window) {
    window->blur_cache.dirty = true;
}
```

### 2. Damage Tracking

```c
void render_window_with_blur_damage(struct compositor_window* window,
                                     pixman_region32_t* damage) {
    // Skip blur if damage doesn't intersect window
    if (!pixman_region32_intersect_rect(damage, &window->geometry)) {
        return;
    }

    // Only re-blur if backdrop damaged
    pixman_region32_t backdrop_damage;
    pixman_region32_init(&backdrop_damage);
    pixman_region32_intersect(&backdrop_damage, damage, &window->backdrop_region);

    if (pixman_region32_not_empty(&backdrop_damage)) {
        // Backdrop changed, re-request blur
        struct wlr_buffer* blurred = get_or_render_blur(window);
        update_blur_layer(window, blurred);
        wlr_buffer_drop(blurred);
    }

    pixman_region32_fini(&backdrop_damage);
}
```

---

## Testing Strategies

### 1. Integration Testing

```c
// Test blur client connection
void test_blur_client_init(void) {
    struct blur_client* client = blur_client_init();
    assert(client != NULL);
    assert(client->client_id > 0);
    blur_client_destroy(client);
}

// Test blur node lifecycle
void test_blur_node_lifecycle(void) {
    struct blur_client* client = blur_client_init();

    uint32_t blur_id = blur_create_node(client, 1920, 1080);
    assert(blur_id > 0);

    blur_destroy_node(client, blur_id);
    blur_client_destroy(client);
}

// Test blur request
void test_blur_request(void) {
    struct blur_client* client = blur_client_init();
    uint32_t blur_id = blur_create_node(client, 1920, 1080);

    // Create test backdrop (solid color)
    struct wlr_buffer* backdrop = create_test_buffer(1920, 1080, 0xFFFF0000);

    struct wlr_buffer* blurred = blur_request(client, blur_id, backdrop);
    assert(blurred != NULL);

    // Verify blurred buffer dimensions
    assert(blurred->width == 1920);
    assert(blurred->height == 1080);

    wlr_buffer_drop(blurred);
    wlr_buffer_drop(backdrop);
    blur_destroy_node(client, blur_id);
    blur_client_destroy(client);
}
```

### 2. Visual Testing

```bash
# Test 1: Solid color blur
compositor --blur-test-mode=solid_red
# Expected: Red blurred to pink gradient

# Test 2: Gradient blur
compositor --blur-test-mode=rainbow_gradient
# Expected: Smooth blurred gradient

# Test 3: Performance test
compositor --blur-test-mode=100_windows
# Expected: <2ms per frame, no stutter
```

---

## Troubleshooting

### Common Issues

**1. Daemon Not Available**
```
Error: Failed to connect to blur daemon
```
**Solution:** Start daemon: `systemctl --user start wlblur.service`

**2. DMA-BUF Export Failed**
```
Error: wlr_buffer_get_dmabuf returned false
```
**Solution:** Check compositor DMA-BUF support, verify GPU driver

**3. Blur Artifacts**
```
Visual: Edges of blur region show seams
```
**Solution:** Increase damage region padding by blur radius

**4. Performance Issues**
```
Symptom: Framerate drops below 60 FPS
```
**Solution:**
- Enable blur caching
- Reduce blur passes (2 → 1)
- Implement damage tracking

---

## Future Enhancements

### 1. Wayland Protocol Extension

```xml
<!-- wlblur-v1.xml -->
<protocol name="wlblur_v1">
  <interface name="wlblur_manager_v1" version="1">
    <request name="create_blur_surface">
      <arg name="id" type="new_id" interface="wlblur_surface_v1"/>
      <arg name="surface" type="object" interface="wl_surface"/>
    </request>
  </interface>

  <interface name="wlblur_surface_v1" version="1">
    <request name="set_blur_radius">
      <arg name="radius" type="uint"/>
    </request>
    <request name="set_algorithm">
      <arg name="algorithm" type="uint"/>
    </request>
    <request name="destroy" type="destructor"/>
  </interface>
</protocol>
```

### 2. Material System Integration

```c
// Future: Apple-style material presets
enum blur_material {
    BLUR_MATERIAL_HUD,          // Deep blur, high vibrancy
    BLUR_MATERIAL_SIDEBAR,      // Medium blur, neutral
    BLUR_MATERIAL_POPOVER,      // Light blur, adaptive tint
    BLUR_MATERIAL_TOOLBAR,      // Minimal blur, high opacity
};

void set_window_material(struct compositor_window* window,
                         enum blur_material material);
```

---

## References

### Related Docs
- [00-overview.md](00-overview) - System architecture
- [01-libwlblur.md](01-libwlblur) - Library internals
- [02-wlblurd.md](02-wlblurd) - Daemon architecture

### Investigation Sources
- `docs/post-investigation/blur-daemon-approach.md` - Integration rationale
- `integrations/scroll/` - Reference implementation (future)

---

**End of Architecture Documentation**
