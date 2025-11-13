# SceneFX to External Daemon Translation Guide

**Investigation Date:** November 12, 2025
**Purpose:** Map in-process SceneFX operations to IPC daemon architecture

---

## Architecture Comparison

### In-Process (SceneFX)

```
┌─────────────────────────────────────────────────────┐
│ Compositor Process                                   │
│                                                      │
│  ┌─────────────────┐                                │
│  │ Scene Graph     │                                │
│  │ - blur nodes    │                                │
│  │ - buffer nodes  │                                │
│  └────────┬────────┘                                │
│           │                                          │
│           ↓                                          │
│  ┌─────────────────┐       ┌──────────────────────┐ │
│  │ FX Renderer     │←─────→│ Effect Framebuffers  │ │
│  │ - blur shaders  │       │ - ping-pong buffers  │ │
│  │ - multi-pass    │       │ - cached blur        │ │
│  └────────┬────────┘       └──────────────────────┘ │
│           │                                          │
│           ↓                                          │
│  ┌─────────────────┐                                │
│  │ OpenGL Context  │                                │
│  │ - shared EGL    │                                │
│  └─────────────────┘                                │
└─────────────────────────────────────────────────────┘
```

### Out-of-Process (Daemon)

```
┌──────────────────────────────────────┐     ┌─────────────────────────────────────┐
│ Compositor Process                    │     │ Blur Daemon Process                 │
│                                       │     │                                     │
│  ┌─────────────────┐                 │     │  ┌──────────────────────────┐      │
│  │ Scene Graph     │                 │     │  │ Virtual Scene Graph      │      │
│  │ - blur nodes    │─────── IPC ────────────→│ - blur node registry     │      │
│  │ - buffer nodes  │                 │     │  │ - buffer ID mapping      │      │
│  └────────┬────────┘                 │     │  └──────────┬───────────────┘      │
│           │                           │     │             │                       │
│           ↓                           │     │             ↓                       │
│  ┌─────────────────┐                 │     │  ┌──────────────────────────┐      │
│  │ Standard        │                 │     │  │ FX Renderer              │      │
│  │ Renderer        │←── DMA-BUF FD ─────────→│ - blur shaders           │      │
│  │ - no blur       │                 │     │  │ - multi-pass             │      │
│  │ - compositing   │                 │     │  │ - damage expansion       │      │
│  └─────────────────┘                 │     │  └──────────┬───────────────┘      │
│                                       │     │             │                       │
│  ┌─────────────────┐                 │     │  ┌──────────────────────────┐      │
│  │ OpenGL Context  │                 │     │  │ Separate OpenGL Context  │      │
│  │ (compositor GL) │                 │     │  │ (daemon GL)              │      │
│  └─────────────────┘                 │     │  └──────────────────────────┘      │
└──────────────────────────────────────┘     └─────────────────────────────────────┘
```

---

## Operation Translation Matrix

### 1. Blur Node Creation

**In-Process (SceneFX):**
```c
// File: types/scene/wlr_scene.c (lines 1034-1056)
struct wlr_scene_blur *wlr_scene_blur_create(struct wlr_scene_tree *parent,
    int width, int height) {

    struct wlr_scene_blur *blur = calloc(1, sizeof(*blur));
    scene_node_init(&blur->node, WLR_SCENE_NODE_BLUR, parent);

    blur->alpha = 1.0f;
    blur->strength = 1.0f;
    blur->corner_radius = 0;
    blur->should_only_blur_bottom_layer = false;
    blur->width = width;
    blur->height = height;

    linked_node_init(&blur->transparency_mask_source);

    scene_node_update(&blur->node, NULL);
    return blur;
}
```

**Out-of-Process (Daemon IPC):**

**Compositor Side:**
```c
// Create local tracking structure
struct compositor_blur_node {
    uint32_t blur_id;       // Daemon-assigned ID
    uint32_t parent_id;     // Scene tree parent
    int width, height;
    float alpha, strength;
    int corner_radius;
    bool should_only_blur_bottom_layer;
    uint32_t mask_buffer_id;  // Linked surface buffer ID
};

struct compositor_blur_node *create_blur_node_ipc(uint32_t parent_id,
    int width, int height) {

    // Allocate request
    struct blur_create_req req = {
        .op = BLUR_OP_CREATE_NODE,
        .parent_id = parent_id,
        .width = width,
        .height = height,
    };

    // Send to daemon via Unix socket
    send(daemon_sock, &req, sizeof(req), 0);

    // Receive response with blur_id
    struct blur_create_resp resp;
    recv(daemon_sock, &resp, sizeof(resp), 0);

    // Create local tracking
    struct compositor_blur_node *blur = calloc(1, sizeof(*blur));
    blur->blur_id = resp.blur_id;
    blur->parent_id = parent_id;
    blur->width = width;
    blur->height = height;
    blur->alpha = 1.0f;
    blur->strength = 1.0f;

    return blur;
}
```

**Daemon Side:**
```c
void handle_blur_create_req(struct blur_daemon *daemon,
    struct blur_create_req *req) {

    // Allocate daemon-side blur node
    struct daemon_blur_node *blur = calloc(1, sizeof(*blur));
    blur->id = ++daemon->next_blur_id;
    blur->parent_id = req->parent_id;
    blur->width = req->width;
    blur->height = req->height;
    blur->alpha = 1.0f;
    blur->strength = 1.0f;

    // Insert into registry
    hash_table_insert(daemon->blur_nodes, blur->id, blur);

    // Send response
    struct blur_create_resp resp = {
        .blur_id = blur->id,
    };
    send(client_sock, &resp, sizeof(resp), 0);
}
```

---

### 2. DMA-BUF Texture Import

**In-Process (SceneFX):**
```c
// File: render/fx_renderer/fx_texture.c (lines 353-404)
static struct wlr_texture *fx_texture_from_dmabuf(
    struct wlr_renderer *wlr_renderer,
    struct wlr_buffer *wlr_buffer,
    struct wlr_dmabuf_attributes *attribs) {

    struct fx_renderer *renderer = fx_get_renderer(wlr_renderer);

    // Get or create framebuffer from DMA-BUF
    struct fx_framebuffer *buffer = fx_framebuffer_get_or_create(renderer, wlr_buffer);

    // Create texture wrapper
    struct fx_texture *texture = fx_texture_create(renderer,
        attribs->width, attribs->height);

    texture->target = buffer->external_only ? GL_TEXTURE_EXTERNAL_OES : GL_TEXTURE_2D;
    texture->buffer = buffer;
    texture->has_alpha = pixel_format_has_alpha(attribs->format);

    // Bind EGLImage to GL texture
    glBindTexture(texture->target, buffer->tex);
    renderer->procs.glEGLImageTargetTexture2DOES(texture->target, buffer->image);
    glBindTexture(texture->target, 0);

    wlr_buffer_lock(texture->buffer->buffer);  // Reference count
    return &texture->wlr_texture;
}
```

**Out-of-Process (Daemon IPC):**

**Compositor Side:**
```c
struct compositor_buffer {
    uint32_t buffer_id;       // Daemon-assigned ID
    struct wlr_buffer *wlr_buffer;
    struct wlr_dmabuf_attributes dmabuf;
};

uint32_t import_buffer_to_daemon(struct wlr_buffer *wlr_buffer) {
    struct wlr_dmabuf_attributes dmabuf;
    if (!wlr_buffer_get_dmabuf(wlr_buffer, &dmabuf)) {
        return 0;  // Failed to get DMA-BUF
    }

    // Build request with DMA-BUF metadata
    struct blur_import_dmabuf_req req = {
        .op = BLUR_OP_IMPORT_DMABUF,
        .width = dmabuf.width,
        .height = dmabuf.height,
        .format = dmabuf.format,
        .n_planes = dmabuf.n_planes,
    };

    for (int i = 0; i < dmabuf.n_planes; i++) {
        req.offsets[i] = dmabuf.offset[i];
        req.strides[i] = dmabuf.stride[i];
        req.modifiers[i] = dmabuf.modifier;
    }

    // Send request
    send(daemon_sock, &req, sizeof(req), 0);

    // Send FDs via SCM_RIGHTS ancillary data
    struct msghdr msg = {0};
    struct cmsghdr *cmsg;
    char cmsg_buf[CMSG_SPACE(sizeof(int) * dmabuf.n_planes)];

    msg.msg_control = cmsg_buf;
    msg.msg_controllen = sizeof(cmsg_buf);

    cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int) * dmabuf.n_planes);

    memcpy(CMSG_DATA(cmsg), dmabuf.fd, sizeof(int) * dmabuf.n_planes);

    sendmsg(daemon_sock, &msg, 0);

    // Receive response with buffer_id
    struct blur_import_dmabuf_resp resp;
    recv(daemon_sock, &resp, sizeof(resp), 0);

    return resp.buffer_id;
}
```

**Daemon Side:**
```c
void handle_import_dmabuf_req(struct blur_daemon *daemon,
    struct blur_import_dmabuf_req *req, int *fds, int n_fds) {

    // Create EGLImage from DMA-BUF
    EGLint attribs[50];
    int atti = 0;

    attribs[atti++] = EGL_WIDTH;
    attribs[atti++] = req->width;
    attribs[atti++] = EGL_HEIGHT;
    attribs[atti++] = req->height;
    attribs[atti++] = EGL_LINUX_DRM_FOURCC_EXT;
    attribs[atti++] = req->format;

    for (int i = 0; i < req->n_planes; i++) {
        attribs[atti++] = EGL_DMA_BUF_PLANE0_FD_EXT + i * 3;
        attribs[atti++] = fds[i];
        attribs[atti++] = EGL_DMA_BUF_PLANE0_OFFSET_EXT + i * 3;
        attribs[atti++] = req->offsets[i];
        attribs[atti++] = EGL_DMA_BUF_PLANE0_PITCH_EXT + i * 3;
        attribs[atti++] = req->strides[i];
    }

    attribs[atti++] = EGL_NONE;

    EGLImageKHR image = eglCreateImageKHR(daemon->egl_display,
        EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT, NULL, attribs);

    // Create daemon buffer object
    struct daemon_buffer *buffer = calloc(1, sizeof(*buffer));
    buffer->id = ++daemon->next_buffer_id;
    buffer->image = image;
    buffer->width = req->width;
    buffer->height = req->height;

    // Create GL texture from EGLImage
    glGenTextures(1, &buffer->tex);
    glBindTexture(GL_TEXTURE_2D, buffer->tex);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, image);
    glBindTexture(GL_TEXTURE_2D, 0);

    // Insert into registry
    hash_table_insert(daemon->buffers, buffer->id, buffer);

    // Send response
    struct blur_import_dmabuf_resp resp = {
        .buffer_id = buffer->id,
    };
    send(client_sock, &resp, sizeof(resp), 0);

    // Close FDs (daemon owns them now via EGLImage)
    for (int i = 0; i < n_fds; i++) {
        close(fds[i]);
    }
}
```

---

### 3. Blur Rendering

**In-Process (SceneFX):**
```c
// File: render/fx_renderer/fx_pass.c (lines 869-947)
static void get_main_buffer_blur(struct fx_gles_render_pass *pass,
    const struct fx_render_blur_pass_options *fx_options,
    pixman_region32_t *damage, int blur_width, int blur_height) {

    struct fx_renderer *renderer = pass->buffer->renderer;

    // Apply strength
    struct blur_data blur_data = blur_data_apply_strength(
        fx_options->blur_data, fx_options->blur_strength);

    // Expand damage
    wlr_region_expand(damage, damage, blur_data_calc_size(&blur_data));

    // Downsample passes
    for (int i = 0; i < blur_data.num_passes; ++i) {
        render_blur_segments(pass, fx_options, &renderer->shaders.blur1, i);
        swap_buffers(fx_options);
    }

    // Upsample passes
    for (int i = blur_data.num_passes - 1; i >= 0; --i) {
        render_blur_segments(pass, fx_options, &renderer->shaders.blur2, i);
        swap_buffers(fx_options);
    }

    // Post-processing
    if (blur_data_should_parameters_blur_effects(&blur_data)) {
        render_blur_effects(pass, fx_options, &renderer->shaders.blur_effects);
    }
}
```

**Out-of-Process (Daemon IPC):**

**Compositor Side:**
```c
uint32_t request_blur_render(uint32_t buffer_id, uint32_t blur_id,
    pixman_region32_t *damage) {

    // Expand damage locally (compositor knows blur size)
    struct blur_data blur_data = get_blur_data();  // Global or per-blur
    int blur_size = blur_data_calc_size(&blur_data);
    wlr_region_expand(damage, damage, blur_size);

    // Convert damage region to array of boxes
    int n_rects;
    const pixman_box32_t *rects = pixman_region32_rectangles(damage, &n_rects);

    // Build request
    struct blur_render_req req = {
        .op = BLUR_OP_RENDER,
        .buffer_id = buffer_id,
        .blur_id = blur_id,
        .n_damage_rects = n_rects,
    };

    send(daemon_sock, &req, sizeof(req), 0);

    // Send damage rectangles
    send(daemon_sock, rects, sizeof(pixman_box32_t) * n_rects, 0);

    // Receive response with blurred buffer ID
    struct blur_render_resp resp;
    recv(daemon_sock, &resp, sizeof(resp), 0);

    return resp.blurred_buffer_id;
}
```

**Daemon Side:**
```c
void handle_blur_render_req(struct blur_daemon *daemon,
    struct blur_render_req *req, pixman_box32_t *damage_rects, int n_rects) {

    // Lookup source buffer and blur node
    struct daemon_buffer *src_buffer = hash_table_get(daemon->buffers, req->buffer_id);
    struct daemon_blur_node *blur_node = hash_table_get(daemon->blur_nodes, req->blur_id);

    // Allocate/reuse output buffer
    struct daemon_buffer *dst_buffer = get_or_create_buffer(daemon,
        src_buffer->width, src_buffer->height);

    // Build damage region
    pixman_region32_t damage;
    pixman_region32_init_rects(&damage, damage_rects, n_rects);

    // Apply blur (multi-pass)
    apply_dual_kawase_blur(daemon, src_buffer, dst_buffer,
        &daemon->blur_params, &damage, blur_node->strength);

    pixman_region32_fini(&damage);

    // Send response
    struct blur_render_resp resp = {
        .blurred_buffer_id = dst_buffer->id,
    };
    send(client_sock, &resp, sizeof(resp), 0);
}
```

---

### 4. Blur-to-Surface Linking

**In-Process (SceneFX):**
```c
// File: types/scene/wlr_scene.c (lines 1090-1108)
void wlr_scene_blur_set_transparency_mask_source(struct wlr_scene_blur *blur,
    struct wlr_scene_buffer *source) {

    if (source == NULL) {
        linked_node_destroy(&blur->transparency_mask_source);
        return;
    }

    // Unlink old
    linked_node_destroy(&blur->transparency_mask_source);
    linked_node_destroy(&source->blur);

    // Create new bidirectional link
    linked_node_init_link(&blur->transparency_mask_source, &source->blur);

    scene_node_update(&blur->node, NULL);
}
```

**Out-of-Process (Daemon IPC):**

**Compositor Side:**
```c
void link_blur_to_surface_ipc(uint32_t blur_id, uint32_t buffer_id) {
    struct blur_link_req req = {
        .op = BLUR_OP_LINK_TO_SURFACE,
        .blur_id = blur_id,
        .buffer_id = buffer_id,
    };

    send(daemon_sock, &req, sizeof(req), 0);

    struct blur_link_resp resp;
    recv(daemon_sock, &resp, sizeof(resp), 0);

    // Update local tracking
    struct compositor_blur_node *blur = find_blur_node(blur_id);
    blur->mask_buffer_id = buffer_id;
}

void unlink_blur_from_surface_ipc(uint32_t blur_id) {
    struct blur_link_req req = {
        .op = BLUR_OP_UNLINK_FROM_SURFACE,
        .blur_id = blur_id,
        .buffer_id = 0,
    };

    send(daemon_sock, &req, sizeof(req), 0);

    // Update local tracking
    struct compositor_blur_node *blur = find_blur_node(blur_id);
    blur->mask_buffer_id = 0;
}
```

**Daemon Side:**
```c
void handle_blur_link_req(struct blur_daemon *daemon,
    struct blur_link_req *req) {

    struct daemon_blur_node *blur = hash_table_get(daemon->blur_nodes, req->blur_id);

    if (req->buffer_id == 0) {
        // Unlink
        blur->mask_buffer_id = 0;
    } else {
        // Link to buffer
        blur->mask_buffer_id = req->buffer_id;
    }

    struct blur_link_resp resp = { .success = true };
    send(client_sock, &resp, sizeof(resp), 0);
}

// During blur rendering:
void apply_blur_with_mask(struct daemon_buffer *src, struct daemon_buffer *dst,
    struct daemon_blur_node *blur) {

    if (blur->mask_buffer_id != 0) {
        // Render mask to stencil buffer
        struct daemon_buffer *mask = hash_table_get(daemon->buffers, blur->mask_buffer_id);
        render_to_stencil(mask);

        // Enable stencil test
        glEnable(GL_STENCIL_TEST);
        glStencilFunc(GL_EQUAL, 1, 0xFF);
    }

    // Render blur (respects stencil if enabled)
    apply_dual_kawase_blur(daemon, src, dst);

    if (blur->mask_buffer_id != 0) {
        glDisable(GL_STENCIL_TEST);
    }
}
```

---

### 5. Resource Cleanup

**In-Process (SceneFX):**
```c
// Automatic via wlr_addon system
static void buffer_addon_destroy(struct wlr_addon *addon) {
    struct fx_framebuffer *buffer = wl_container_of(addon, buffer, addon);

    // Clean up GL resources
    if (buffer->image != EGL_NO_IMAGE_KHR) {
        wlr_egl_destroy_image(buffer->renderer->egl, buffer->image);
    }

    glDeleteTextures(1, &buffer->tex);
    glDeleteRenderbuffers(1, &buffer->rbo);
    glDeleteRenderbuffers(1, &buffer->sb);
    glDeleteFramebuffers(1, &buffer->fbo);

    wl_list_remove(&buffer->link);
    free(buffer);
}
```

**Out-of-Process (Daemon IPC):**

**Compositor Side:**
```c
void destroy_blur_node_ipc(uint32_t blur_id) {
    struct blur_destroy_req req = {
        .op = BLUR_OP_DESTROY_NODE,
        .id = blur_id,
    };

    send(daemon_sock, &req, sizeof(req), 0);

    // Wait for confirmation
    struct blur_destroy_resp resp;
    recv(daemon_sock, &resp, sizeof(resp), 0);

    // Remove local tracking
    struct compositor_blur_node *blur = find_blur_node(blur_id);
    hash_table_remove(compositor->blur_nodes, blur_id);
    free(blur);
}

void release_buffer_ipc(uint32_t buffer_id) {
    struct blur_release_buffer_req req = {
        .op = BLUR_OP_RELEASE_BUFFER,
        .buffer_id = buffer_id,
    };

    send(daemon_sock, &req, sizeof(req), 0);

    // Don't wait for response (async cleanup)
}

// On client disconnect, compositor sends:
void cleanup_client_resources(struct blur_daemon_client *client) {
    struct blur_cleanup_req req = {
        .op = BLUR_OP_CLEANUP_CLIENT,
    };

    send(daemon_sock, &req, sizeof(req), 0);
}
```

**Daemon Side:**
```c
void handle_blur_destroy_req(struct blur_daemon *daemon,
    struct blur_destroy_req *req) {

    struct daemon_blur_node *blur = hash_table_get(daemon->blur_nodes, req->id);
    if (!blur) {
        return;  // Already destroyed
    }

    // Remove from registry
    hash_table_remove(daemon->blur_nodes, req->id);
    free(blur);

    struct blur_destroy_resp resp = { .success = true };
    send(client_sock, &resp, sizeof(resp), 0);
}

void handle_release_buffer_req(struct blur_daemon *daemon,
    struct blur_release_buffer_req *req) {

    struct daemon_buffer *buffer = hash_table_get(daemon->buffers, req->buffer_id);
    if (!buffer) {
        return;
    }

    // Decrement reference count
    buffer->refcount--;

    if (buffer->refcount == 0) {
        // Destroy GL resources
        glDeleteTextures(1, &buffer->tex);
        glDeleteFramebuffers(1, &buffer->fbo);

        if (buffer->image != EGL_NO_IMAGE_KHR) {
            eglDestroyImageKHR(daemon->egl_display, buffer->image);
        }

        // Remove from registry
        hash_table_remove(daemon->buffers, req->buffer_id);
        free(buffer);
    }
}

void handle_cleanup_client_req(struct blur_daemon *daemon,
    struct blur_daemon_client *client) {

    // Destroy all blur nodes owned by this client
    hash_table_foreach(daemon->blur_nodes, blur_node) {
        if (blur_node->client == client) {
            hash_table_remove(daemon->blur_nodes, blur_node->id);
            free(blur_node);
        }
    }

    // Destroy all buffers owned by this client
    hash_table_foreach(daemon->buffers, buffer) {
        if (buffer->client == client) {
            // Clean up GL resources
            glDeleteTextures(1, &buffer->tex);
            glDeleteFramebuffers(1, &buffer->fbo);
            eglDestroyImageKHR(daemon->egl_display, buffer->image);

            hash_table_remove(daemon->buffers, buffer->id);
            free(buffer);
        }
    }
}
```

---

## IPC Protocol Definition

### Message Structure

```c
#define BLUR_OP_CREATE_NODE          1
#define BLUR_OP_DESTROY_NODE         2
#define BLUR_OP_IMPORT_DMABUF        3
#define BLUR_OP_RELEASE_BUFFER       4
#define BLUR_OP_RENDER               5
#define BLUR_OP_LINK_TO_SURFACE      6
#define BLUR_OP_UNLINK_FROM_SURFACE  7
#define BLUR_OP_SET_PARAMETERS       8
#define BLUR_OP_CLEANUP_CLIENT       9

struct blur_req_header {
    uint32_t op;
    uint32_t req_id;  // For matching responses
    uint32_t payload_size;
};

struct blur_resp_header {
    uint32_t req_id;
    int32_t error_code;  // 0 = success
    uint32_t payload_size;
};
```

### Request/Response Pairs

```c
// Create blur node
struct blur_create_req {
    struct blur_req_header header;
    uint32_t parent_id;
    int width, height;
};

struct blur_create_resp {
    struct blur_resp_header header;
    uint32_t blur_id;
};

// Import DMA-BUF
struct blur_import_dmabuf_req {
    struct blur_req_header header;
    uint32_t width, height;
    uint32_t format;
    uint8_t n_planes;
    uint32_t offsets[4];
    uint32_t strides[4];
    uint64_t modifiers[4];
    // FDs sent via SCM_RIGHTS
};

struct blur_import_dmabuf_resp {
    struct blur_resp_header header;
    uint32_t buffer_id;
};

// Render blur
struct blur_render_req {
    struct blur_req_header header;
    uint32_t buffer_id;
    uint32_t blur_id;
    uint32_t n_damage_rects;
    // Followed by: pixman_box32_t damage_rects[n_damage_rects]
};

struct blur_render_resp {
    struct blur_resp_header header;
    uint32_t blurred_buffer_id;
};

// Set blur parameters
struct blur_set_parameters_req {
    struct blur_req_header header;
    uint32_t blur_id;  // 0 = global parameters
    float strength;
    float alpha;
    int corner_radius;
    bool should_only_blur_bottom_layer;
};

struct blur_set_parameters_resp {
    struct blur_resp_header header;
};
```

---

## State Synchronization

### Problem: Shared State Between Processes

In-process SceneFX has direct access to all state. Out-of-process daemon must replicate state.

### Solution: Authoritative Daemon State

**Compositor is source of truth for:**
- Scene graph structure
- Buffer lifecycle (wlr_buffer)
- Damage regions

**Daemon is source of truth for:**
- Blur parameters per node
- GL resources (textures, FBOs, shaders)
- Cached blur state (dirty flags)

**Synchronization points:**
1. **Node creation:** Compositor → Daemon (create node)
2. **Parameter change:** Compositor → Daemon (update parameters)
3. **Buffer import:** Compositor → Daemon (send DMA-BUF FDs)
4. **Rendering:** Compositor → Daemon (render request) → Daemon → Compositor (blurred result)
5. **Cleanup:** Compositor → Daemon (destroy node/buffer)

---

## Threading and Synchronization

### In-Process (SceneFX)

**Single-threaded:**
- Compositor renders on main thread
- All GL calls from same thread
- EGL context: `wlr_egl_make_current()` / `wlr_egl_restore_context()`

### Out-of-Process (Daemon)

**Daemon threading options:**

**Option 1: Single-threaded IPC loop**
```c
while (running) {
    // Poll for IPC messages
    poll(fds, nfds, -1);

    // Process requests
    handle_ipc_message();

    // Render blurs
    render_pending_blurs();
}
```
- **Pros:** Simple, no locking
- **Cons:** Latency (sequential processing)

**Option 2: Multi-threaded with per-client GL contexts**
```c
// Main thread: accept connections
// Per-client thread: dedicated GL context
void *client_thread(void *arg) {
    struct blur_daemon_client *client = arg;

    // Create dedicated EGL context for this client
    EGLContext ctx = eglCreateContext(daemon->egl_display, ...);
    eglMakeCurrent(daemon->egl_display, ..., ctx);

    while (client->connected) {
        // Process client requests
        handle_client_message(client);
    }

    eglDestroyContext(daemon->egl_display, ctx);
}
```
- **Pros:** Parallel processing, low latency
- **Cons:** Complex synchronization, higher memory (multiple contexts)

**Option 3: Async queue with worker thread**
```c
// IPC thread: receives requests, enqueues
// GL thread: processes queue, renders blurs

void *ipc_thread(void *arg) {
    while (running) {
        struct blur_req *req = receive_request();
        queue_push(&daemon->request_queue, req);
    }
}

void *gl_thread(void *arg) {
    eglMakeCurrent(daemon->egl_display, ...);

    while (running) {
        struct blur_req *req = queue_pop(&daemon->request_queue);
        handle_request(req);
    }
}
```
- **Pros:** Good latency, single GL context
- **Cons:** Queue synchronization, memory barriers

**Recommended:** Option 3 (async queue)

---

## Synchronization Primitives

### DMA-BUF Sync Objects

**Problem:** Compositor and daemon both access same GPU memory. Need synchronization.

**Solution:** Use DRM sync objects (syncobj)

**Compositor side:**
```c
// Before sending DMA-BUF to daemon
struct wlr_drm_syncobj_timeline *timeline = wlr_drm_syncobj_timeline_create(...);
uint64_t point = ++timeline->current_point;

// Create fence signaling when compositor is done with buffer
EGLSyncKHR sync = eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
int sync_fd = eglDupNativeFenceFDANDROID(egl_display, sync);
wlr_drm_syncobj_timeline_import_sync_file(timeline, point, sync_fd);

// Send timeline FD to daemon along with DMA-BUF
struct blur_render_req req = {
    .buffer_id = buffer_id,
    .wait_timeline_fd = timeline->drm_syncobj_fd,
    .wait_point = point,
};
```

**Daemon side:**
```c
// Wait for compositor to finish with buffer
struct wlr_drm_syncobj_timeline *wait_timeline =
    wlr_drm_syncobj_timeline_import_drm_syncobj(req->wait_timeline_fd);

wlr_drm_syncobj_timeline_wait(wait_timeline, req->wait_point, TIMEOUT);

// Now safe to access buffer
render_blur(buffer);

// Signal completion
struct wlr_drm_syncobj_timeline *signal_timeline = get_signal_timeline();
uint64_t signal_point = ++signal_timeline->current_point;

EGLSyncKHR sync = eglCreateSyncKHR(egl_display, EGL_SYNC_FENCE_KHR, NULL);
int sync_fd = eglDupNativeFenceFDANDROID(egl_display, sync);
wlr_drm_syncobj_timeline_import_sync_file(signal_timeline, signal_point, sync_fd);

// Send signal timeline to compositor
struct blur_render_resp resp = {
    .blurred_buffer_id = dst_buffer_id,
    .signal_timeline_fd = signal_timeline->drm_syncobj_fd,
    .signal_point = signal_point,
};
```

**Compositor side (after receiving blurred buffer):**
```c
// Wait for daemon to finish rendering
wlr_drm_syncobj_timeline_wait(resp_timeline, resp->signal_point, TIMEOUT);

// Now safe to composite blurred buffer
```

---

## Performance Considerations

### IPC Overhead

**Latency breakdown (estimated):**
```
Compositor → Daemon:
- Unix socket sendmsg():        ~0.05ms
- Context switch:                ~0.01ms
- Daemon processing:             ~0.02ms
Total: ~0.08ms

Daemon → Compositor:
- Daemon sendmsg():              ~0.05ms
- Context switch:                ~0.01ms
- Compositor processing:         ~0.02ms
Total: ~0.08ms

Round-trip IPC overhead:         ~0.16ms
```

**Blur rendering (daemon-side):**
```
DMA-BUF import:                  ~0.1ms (one-time)
Blur rendering (3 passes):       ~1.2ms
DMA-BUF export:                  ~0.1ms
Total: ~1.4ms
```

**Total overhead vs in-process:**
- In-process: ~1.2ms
- Out-of-process: ~1.56ms (30% overhead)

**Optimization: Async Pipeline**

Don't wait for blur completion before continuing:

```c
// Frame N: Start blur for window A
send_blur_request(window_a_buffer);

// Frame N: Continue rendering non-blurred content
render_other_windows();

// Frame N+1: Check if blur completed
if (blur_response_ready(window_a_buffer)) {
    composite_blurred_window(window_a);
}
```

**Result:** Overlap IPC and rendering, hide latency

---

## Error Handling

### In-Process (SceneFX)

Errors are immediate (function returns NULL or false):

```c
struct wlr_texture *tex = fx_texture_from_dmabuf(renderer, buffer, &dmabuf);
if (!tex) {
    wlr_log(WLR_ERROR, "Failed to import DMA-BUF");
    return;
}
```

### Out-of-Process (Daemon IPC)

Errors are asynchronous (response contains error code):

```c
// Compositor side
uint32_t buffer_id = import_buffer_to_daemon(wlr_buffer);
if (buffer_id == 0) {
    // Check error code from response
    wlr_log(WLR_ERROR, "Daemon import failed: %s", error_string(resp.error_code));
    return;
}

// Error codes
#define BLUR_ERROR_NONE                  0
#define BLUR_ERROR_INVALID_BUFFER_ID     1
#define BLUR_ERROR_INVALID_BLUR_ID       2
#define BLUR_ERROR_GL_ERROR              3
#define BLUR_ERROR_OUT_OF_MEMORY         4
#define BLUR_ERROR_INVALID_DMABUF        5
#define BLUR_ERROR_UNSUPPORTED_FORMAT    6
```

### Daemon Crash Recovery

**Problem:** Daemon crashes, compositor loses all blur state.

**Solution:** Keep compositor-side blur node registry:

```c
void handle_daemon_disconnect() {
    wlr_log(WLR_ERROR, "Blur daemon disconnected, attempting restart");

    // Restart daemon
    if (!restart_blur_daemon()) {
        wlr_log(WLR_ERROR, "Failed to restart blur daemon, disabling blur");
        disable_blur();
        return;
    }

    // Re-send all blur nodes to new daemon instance
    hash_table_foreach(compositor->blur_nodes, blur) {
        recreate_blur_node_in_daemon(blur);
    }

    // Re-import all buffers
    hash_table_foreach(compositor->buffers, buffer) {
        reimport_buffer_to_daemon(buffer);
    }

    wlr_log(WLR_INFO, "Blur daemon state restored");
}
```

---

## Key Takeaways

1. **IPC adds ~0.16ms round-trip overhead** - Acceptable for 60fps (16.6ms budget)
2. **DMA-BUF FD passing via SCM_RIGHTS** - Zero-copy GPU memory sharing
3. **Daemon maintains virtual scene graph** - Tracks blur nodes, parameters, linkages
4. **Compositor expands damage before sending** - Daemon doesn't need damage calculation
5. **Sync objects critical for correctness** - Prevent race conditions on GPU memory
6. **Async pipeline hides latency** - Start blur in frame N, composite in frame N+1
7. **Daemon crash recovery possible** - Compositor tracks all state, can recreate
8. **Per-client GL contexts optional** - Single-context async queue simpler

**Implementation Priority:**
1. Basic IPC (create node, import buffer, render, destroy)
2. DMA-BUF import/export
3. Blur rendering pipeline
4. Sync objects for correctness
5. Async pipeline for performance
6. Crash recovery (nice-to-have)
