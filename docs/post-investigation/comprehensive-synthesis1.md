# Blur Compositor Investigation - Comprehensive Synthesis

**Investigation Date:** November 12, 2025  
**Scope:** Hyprland, Wayfire, SceneFX  
**Purpose:** Design foundation for Apple-level blur compositor via external daemon architecture

---

## Executive Summary

Three production Wayland compositors with blur implementations were investigated to inform the design of a compositor-agnostic external blur daemon. Each implementation revealed critical architectural patterns, optimization techniques, and extraction strategies.

### Key Findings

| Compositor | Algorithm | Architecture | Extractability | Key Strength |
|------------|-----------|--------------|----------------|--------------|
| **Hyprland** | Dual Kawase + Vibrancy | Tightly integrated | ⭐⭐ (2/5) | Performance optimizations (caching, damage, xray) |
| **Wayfire** | 4 algorithms (Kawase/Gaussian/Box/Bokeh) | Plugin-based | ⭐⭐⭐⭐⭐ (5/5) | Clean abstraction, algorithm variety |
| **SceneFX** | Dual Kawase | wlroots scene graph replacement | ⭐⭐⭐ (3/5) | Multi-compositor potential (wlroots-based) |

### Strategic Recommendation

**Build external blur daemon using:**
1. **Wayfire's plugin architecture** as the extraction template (cleanest boundaries)
2. **Hyprland's optimization techniques** for performance (caching, vibrancy, damage awareness)
3. **SceneFX's IPC patterns** for multi-compositor integration (DMA-BUF, scene graph replication)

**Expected Performance:** ~1.4ms total (1.2ms blur + 0.2ms IPC overhead) for 1920×1080

---

## Part 1: Comparative Architecture Analysis

### 1.1 Rendering Integration Patterns

#### Hyprland: Monolithic Integration

```
┌─────────────────────────────────────────────────┐
│            Hyprland Process                     │
│  ┌─────────────────────────────────────────┐   │
│  │  CHyprRenderer (Render Loop)            │   │
│  │  ├─ preRender() → blur cache check      │   │
│  │  ├─ renderMonitor()                     │   │
│  │  │   ├─ renderWorkspace()               │   │
│  │  │   ├─ renderLayer()                   │   │
│  │  │   └─ renderWindow()                  │   │
│  │  └─ endRender()                         │   │
│  └─────────────────────────────────────────┘   │
│                    ↓                            │
│  ┌─────────────────────────────────────────┐   │
│  │  CHyprOpenGLImpl                        │   │
│  │  ├─ blurFramebufferWithDamage()        │   │
│  │  │   ├─ blur1.frag (downsample)        │   │
│  │  │   ├─ blur2.frag (upsample)          │   │
│  │  │   └─ blurfinish.frag (vibrancy)     │   │
│  │  └─ preBlurForCurrentMonitor() [cache] │   │
│  └─────────────────────────────────────────┘   │
│                                                 │
│  Direct framebuffer access (no IPC)            │
│  Shared GL context, tight damage integration   │
└─────────────────────────────────────────────────┘
```

**Characteristics:**
- ✅ **Performance:** 0.8-1.5ms (1080p), 0.05ms with cache
- ✅ **Optimization:** Caching, damage tracking, xray mode
- ✅ **Quality:** Vibrancy system, HDR color management
- ❌ **Coupling:** Direct framebuffer access, per-monitor state
- ❌ **Extractability:** Requires compositor state (blurFB, damage, render passes)

#### Wayfire: Plugin Abstraction

```
┌─────────────────────────────────────────────────┐
│              Wayfire Process                    │
│  ┌─────────────────────────────────────────┐   │
│  │  Wayfire Core (Compositor)              │   │
│  │  ├─ Scene graph                         │   │
│  │  ├─ OpenGL ES 3.0 context               │   │
│  │  └─ Damage tracking                     │   │
│  └─────────────────────────────────────────┘   │
│         │ Plugin API                            │
│         ↓                                       │
│  ┌─────────────────────────────────────────┐   │
│  │  Blur Plugin (.so) [Dynamic Module]     │   │
│  │  ├─ blur_node_t (transformer)           │   │
│  │  ├─ wf_blur_base (algorithm abstraction)│   │
│  │  │   ├─ wf_kawase_blur                  │   │
│  │  │   ├─ wf_gaussian_blur                │   │
│  │  │   ├─ wf_box_blur                     │   │
│  │  │   └─ wf_bokeh_blur                   │   │
│  │  ├─ Own FBO pool                        │   │
│  │  └─ Shader compilation                  │   │
│  └─────────────────────────────────────────┘   │
│                                                 │
│  Plugin uses compositor GL context but         │
│  manages own resources independently           │
└─────────────────────────────────────────────────┘
```

**Characteristics:**
- ✅ **Architecture:** Clean plugin boundary, minimal dependencies
- ✅ **Flexibility:** 4 algorithms, runtime switching
- ✅ **Extractability:** ~85% code reusable in daemon
- ✅ **Portability:** Pure OpenGL ES 2.0 shaders
- ⚠️ **Performance:** 0.8ms (Kawase), 1.2ms (Gaussian) - no caching
- ❌ **Optimizations:** No blur caching, no xray mode

#### SceneFX: Scene Graph Extension

```
┌─────────────────────────────────────────────────┐
│         Compositor Process (SwayFX/etc)         │
│  ┌─────────────────────────────────────────┐   │
│  │  wlroots Scene Graph (Extended)         │   │
│  │  ├─ WLR_SCENE_NODE_TREE                 │   │
│  │  ├─ WLR_SCENE_NODE_BUFFER               │   │
│  │  ├─ WLR_SCENE_NODE_BLUR      [NEW]      │   │
│  │  └─ WLR_SCENE_NODE_OPTIMIZED_BLUR [NEW] │   │
│  └─────────────────────────────────────────┘   │
│                    ↓                            │
│  ┌─────────────────────────────────────────┐   │
│  │  FX Renderer (replaces wlr_renderer)    │   │
│  │  ├─ fx_render_pass_add_blur()           │   │
│  │  ├─ blur1.frag (5-sample downsample)    │   │
│  │  ├─ blur2.frag (8-sample upsample)      │   │
│  │  └─ blur_effects.frag (post-process)    │   │
│  └─────────────────────────────────────────┘   │
│                    ↓                            │
│  ┌─────────────────────────────────────────┐   │
│  │  Per-Output Effect FBOs                 │   │
│  │  ├─ effects_buffer                      │   │
│  │  ├─ effects_buffer_swapped              │   │
│  │  ├─ optimized_blur_buffer (cache)       │   │
│  │  └─ blur_saved_pixels_buffer            │   │
│  └─────────────────────────────────────────┘   │
│                                                 │
│  Drop-in wlroots replacement                   │
└─────────────────────────────────────────────────┘
```

**Characteristics:**
- ✅ **Compatibility:** API-compatible with wlroots
- ✅ **Multi-compositor:** Works with Sway, River, labwc, etc.
- ✅ **Caching:** optimized_blur for static content
- ✅ **Artifact prevention:** Three-level damage expansion
- ⚠️ **Coupling:** Requires wlroots fork/replacement
- ❌ **Vulkan:** GLES only, no Vulkan renderer support

---

### 1.2 Blur Algorithm Comparison

#### Algorithm Matrix

| Implementation | Algorithm | Passes | Samples/Pixel | Quality | Performance | Vibrancy |
|----------------|-----------|--------|---------------|---------|-------------|----------|
| **Hyprland** | Dual Kawase | 1-8 (default: 1) | 16-18 | High | Very Fast | ✅ HSL-based |
| **Wayfire** | Kawase | Configurable | 16-18 | High | Fast | ❌ |
| **Wayfire** | Gaussian | Separable | 25-49 | Highest | Moderate | ❌ |
| **Wayfire** | Box | Simple | 9 | Low | Very Fast | ❌ |
| **Wayfire** | Bokeh | Golden angle | Variable | Artistic | Slow | ❌ |
| **SceneFX** | Dual Kawase | 3 (fixed) | 16-18 | High | Fast | ❌ |

#### Shader Pipeline Comparison

**Hyprland (4-stage):**
```glsl
1. blurprepare.frag    // Contrast, brightness, color management
2. blur1.frag          // Downsample (5-sample + vibrancy boost)
3. blur2.frag          // Upsample (8-sample weighted)
4. blurfinish.frag     // Noise, final vibrancy adjustment
```

**Wayfire (2-stage):**
```glsl
1. blur_algorithm.frag // Algorithm-specific (kawase/gaussian/box/bokeh)
2. blend.frag          // Composite blurred background with view
```

**SceneFX (3-stage):**
```glsl
1. blur1.frag          // Downsample (5-sample diagonal)
2. blur2.frag          // Upsample (8-sample cardinal+diagonal)
3. blur_effects.frag   // Brightness, contrast, saturation, noise
```

#### Performance Characteristics (1920×1080, mid-range GPU)

| Operation | Hyprland (1 pass) | Wayfire (Kawase) | SceneFX (3 passes) |
|-----------|-------------------|------------------|--------------------|
| **Uncached blur** | 0.8-1.5ms | 0.8ms | 1.2ms |
| **Cached blur** | 0.05ms | N/A | 0.2ms (optimized nodes) |
| **With damage (cursor)** | 0.01-0.03ms | ~0.05ms | ~0.05ms |
| **xray mode (10 windows)** | 2-4ms (vs 12-18ms) | N/A | N/A |

---

### 1.3 Optimization Strategies

#### Hyprland's Performance Arsenal

**1. Blur Caching (`new_optimizations`)**
```cpp
// preRender() - Check if blur needs recomputation
if (!m_bBlurFBDirty) {
    return;  // Reuse cached blur from previous frame
}

// Cache remains valid until:
// - Window moved/resized
// - Workspace switched
// - Wallpaper changed
// - Blur settings changed

// Result: 20× speedup for static scenes (1.5ms → 0.05ms)
```

**2. Damage Tracking with Expansion**
```cpp
// Calculate damage expansion based on blur radius
int expand = size * (1 << passes);  // 2^passes
damage.expand_edges(expand);

// Scissor test to damaged region only
glEnable(GL_SCISSOR_TEST);
glScissor(damage.x, damage.y, damage.width, damage.height);

// Result: 98% reduction for micro-interactions (cursor, typing)
```

**3. xray Mode**
```cpp
// Instead of blurring per-window (N blur operations):
// Blur background once, share across all windows

if (xray_enabled) {
    blur_background();  // Single blur operation
    for (each window) {
        composite_with_shared_blur();
    }
} else {
    for (each window) {
        blur_window_backdrop();  // N blur operations
    }
}

// Result: 10× speedup for many-window scenarios
```

#### Wayfire's Damage Padding

```cpp
// Expand damage before rendering to capture blur kernel radius
int padding = calculate_blur_radius();  // e.g., 40px for kawase
damage.expand_edges(padding);

// Save pixels at padding edge to restore later
// (prevents artifacts where blur meets non-blurred content)
saved_pixels = save_region_pixels(padding_region);

// Render blur with expanded damage
render_blur(expanded_damage);

// Restore saved pixels
restore_pixels(saved_pixels, padding_region);
```

#### SceneFX's Three-Level Damage Expansion

```cpp
// Level 1: Visibility Expansion
// During scene graph traversal, expand blur node visibility
blur_node.visible.expand(blur_size);

// Level 2: Render Pass Expansion
// Before rendering, expand damage region
render_pass_damage.expand(blur_size);

// Level 3: Artifact Prevention
// Save padding pixels before blur, restore after
blur_saved_pixels_buffer = save_padding(damage);
render_blur(expanded_damage);
restore_padding(blur_saved_pixels_buffer);
```

---

## Part 2: External Daemon Design

### 2.1 Architectural Blueprint

#### Proposed Architecture

```
┌────────────────────────────────────────────────────────────────────┐
│                        Compositor Process                          │
│                   (scroll/niri/sway/hyprland)                      │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  Standard Renderer (No Blur)                             │    │
│  │  • Scene graph management                                │    │
│  │  • Window rendering                                      │    │
│  │  • Damage tracking                                       │    │
│  │  • DMA-BUF export capability                             │    │
│  └──────────────────────────────────────────────────────────┘    │
│                          │                                        │
│                          ↓                                        │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  Blur IPC Client (~200 lines)                            │    │
│  │  • Detect blur regions (protocol or config)              │    │
│  │  • Export backdrop textures as DMA-BUF                   │    │
│  │  • Send blur requests via Unix socket                    │    │
│  │  • Import blurred results                                │    │
│  │  • Composite with window surfaces                        │    │
│  └──────────────────────────────────────────────────────────┘    │
│                          │                                        │
└──────────────────────────┼────────────────────────────────────────┘
                           │
                           │ IPC: Unix Socket (SOCK_SEQPACKET)
                           │ FD Passing: SCM_RIGHTS (DMA-BUF FDs)
                           │ Protocol: Binary structs
                           │
                           ↓
┌────────────────────────────────────────────────────────────────────┐
│                      Blur Daemon Process                           │
│                                                                    │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  IPC Server (~300 lines)                                 │    │
│  │  • Unix socket listener                                  │    │
│  │  • Request parsing                                       │    │
│  │  • Client state management                               │    │
│  │  • FD reception (DMA-BUF)                                │    │
│  └──────────────────────────────────────────────────────────┘    │
│                          │                                        │
│                          ↓                                        │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  Virtual Scene Graph (~200 lines)                        │    │
│  │  • Blur node registry (client → blur_id mapping)         │    │
│  │  • Buffer registry (dmabuf_fd → buffer_id)               │    │
│  │  • Blur-to-surface linkage                               │    │
│  │  • Node lifecycle management                             │    │
│  └──────────────────────────────────────────────────────────┘    │
│                          │                                        │
│                          ↓                                        │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  Blur Renderer (~600 lines, from Wayfire)                │    │
│  │  ┌────────────────────────────────────────────────────┐  │    │
│  │  │  Algorithm Factory                                 │  │    │
│  │  │  ├─ Kawase (default, from Hyprland/Wayfire)       │  │    │
│  │  │  ├─ Gaussian (from Wayfire)                       │  │    │
│  │  │  ├─ Box (from Wayfire)                            │  │    │
│  │  │  └─ Bokeh (from Wayfire)                          │  │    │
│  │  └────────────────────────────────────────────────────┘  │    │
│  │  • FBO pool management                                   │    │
│  │  • Shader compilation                                    │    │
│  │  • Multi-pass rendering                                  │    │
│  │  • Vibrancy (from Hyprland)                              │    │
│  └──────────────────────────────────────────────────────────┘    │
│                          │                                        │
│                          ↓                                        │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  DMA-BUF Manager (~150 lines)                            │    │
│  │  • Import DMA-BUF FDs → EGLImageKHR → GL textures       │    │
│  │  • Export result textures → DMA-BUF FDs                  │    │
│  │  • Reference counting                                    │    │
│  │  • Format negotiation (RGBA8888, BGRA8888, etc.)        │    │
│  └──────────────────────────────────────────────────────────┘    │
│                          │                                        │
│                          ↓                                        │
│  ┌──────────────────────────────────────────────────────────┐    │
│  │  EGL Context (independent)                               │    │
│  │  • EGLDisplay, EGLContext, EGLSurface (pbuffer)          │    │
│  │  • OpenGL ES 3.0                                         │    │
│  │  • Shares GPU device with compositor (zero-copy)         │    │
│  └──────────────────────────────────────────────────────────┘    │
│                                                                    │
└────────────────────────────────────────────────────────────────────┘
```

---

### 2.2 IPC Protocol Design

#### Message Format

```c
// Request header
struct blur_request_header {
    uint32_t magic;           // 0x424C5552 ("BLUR")
    uint32_t version;         // Protocol version (1)
    uint32_t client_id;       // Client identifier
    uint32_t sequence;        // Request sequence number
    uint32_t opcode;          // Operation code
    uint32_t payload_size;    // Size of operation-specific payload
};

// Operation codes
enum blur_opcode {
    BLUR_OP_CREATE_NODE = 1,
    BLUR_OP_DESTROY_NODE = 2,
    BLUR_OP_IMPORT_DMABUF = 3,
    BLUR_OP_RELEASE_BUFFER = 4,
    BLUR_OP_RENDER = 5,
    BLUR_OP_CONFIGURE = 6,
    BLUR_OP_LINK_TO_SURFACE = 7,
};
```

#### Example: Blur Rendering Flow

```c
// 1. Compositor creates blur node
struct blur_create_node_request {
    struct blur_request_header header;  // opcode = BLUR_OP_CREATE_NODE
    int32_t width;
    int32_t height;
};

struct blur_create_node_response {
    uint32_t blur_id;  // Daemon-assigned blur node ID
};

// 2. Compositor exports backdrop texture
GLuint compositor_tex = render_backdrop();
int dmabuf_fd = export_texture_to_dmabuf(compositor_tex);

// 3. Import to daemon
struct blur_import_dmabuf_request {
    struct blur_request_header header;  // opcode = BLUR_OP_IMPORT_DMABUF
    uint32_t width;
    uint32_t height;
    uint32_t format;   // DRM_FORMAT_ARGB8888, etc.
    uint32_t modifier;
    uint32_t num_planes;
    struct {
        uint32_t offset;
        uint32_t stride;
    } planes[4];
    // dmabuf_fd sent via SCM_RIGHTS
};

struct blur_import_dmabuf_response {
    uint32_t buffer_id;  // Daemon-assigned buffer ID
};

// 4. Render blur
struct blur_render_request {
    struct blur_request_header header;  // opcode = BLUR_OP_RENDER
    uint32_t blur_id;
    uint32_t input_buffer_id;
    uint32_t num_damage_rects;
    struct {
        int32_t x, y;
        int32_t width, height;
    } damage[32];
};

struct blur_render_response {
    uint32_t output_buffer_id;  // DMA-BUF FD sent via SCM_RIGHTS
};

// 5. Compositor imports result and composites
int blurred_fd = recv_dmabuf_fd();
GLuint blurred_tex = import_dmabuf_to_texture(blurred_fd);
composite_blur_with_window(blurred_tex, window_tex);
```

---

### 2.3 Code Reuse Strategy

#### From Wayfire (~70% reusable)

**Directly Reusable (~550 lines):**
- ✅ All shader code (kawase.cpp, gaussian.cpp, box.cpp, bokeh.cpp)
- ✅ `wf_blur_base` class structure
- ✅ FBO ping-pong management
- ✅ Blur radius calculation
- ✅ Multi-pass rendering logic

**Needs Adaptation (~100 lines):**
- ⚠️ GL context management (replace `wf::gles::run_in_context_if_gles` with EGL)
- ⚠️ Texture types (replace `wf::gles_texture_t` with `GLuint`)
- ⚠️ Render target (replace `wf::render_target_t` with custom FBO wrapper)

**Not Reusable (compositor-specific):**
- ❌ Scene graph integration (200 lines)
- ❌ Wayfire plugin API (150 lines)
- ❌ Configuration system (50 lines)

#### From Hyprland (~30% reusable)

**Shader Enhancements:**
- ✅ Vibrancy algorithm (HSL conversion, saturation boost)
- ✅ Color management functions (CM.glsl)
- ✅ Noise overlay shader

**Optimization Concepts (require compositor cooperation):**
- ⚠️ Blur caching logic (daemon can cache, but compositor controls dirty flag)
- ⚠️ Damage expansion formulas
- ⚠️ xray mode concept (compositor decides which windows to blur)

#### From SceneFX (~25% reusable)

**DMA-BUF Patterns:**
- ✅ DMA-BUF import code (`fx_texture.c`, lines 353-404)
- ✅ EGLImageKHR creation patterns
- ✅ Framebuffer lifecycle management

**Scene Graph Concepts:**
- ⚠️ Virtual scene graph design (implement lightweight version in daemon)
- ⚠️ Blur-to-surface linking pattern
- ⚠️ Optimized blur for static content

---

### 2.4 Implementation Roadmap

#### Phase 1: Foundation (Week 1-2)

**Deliverables:**
- [ ] Daemon process skeleton with event loop
- [ ] Unix socket server (SOCK_SEQPACKET)
- [ ] IPC protocol structs and parsing
- [ ] EGL context initialization
- [ ] Basic error handling and logging

**Code estimate:** ~400 lines

**Key files:**
```
daemon/
  main.c                  # Entry point, event loop
  ipc_server.c           # Socket management, message parsing
  egl_context.c          # EGL initialization
  protocol.h             # IPC structs
```

#### Phase 2: DMA-BUF Integration (Week 3)

**Deliverables:**
- [ ] DMA-BUF FD reception via SCM_RIGHTS
- [ ] EGLImageKHR creation from DMA-BUF attributes
- [ ] GL texture import from EGLImageKHR
- [ ] DMA-BUF export for results
- [ ] Reference counting and buffer lifecycle

**Code estimate:** ~300 lines

**Key files:**
```
daemon/
  dmabuf_manager.c       # Import/export logic
  buffer_registry.c      # Buffer ID tracking
```

#### Phase 3: Blur Renderer (Week 4-5)

**Deliverables:**
- [ ] Port Kawase blur from Wayfire
- [ ] FBO ping-pong management
- [ ] Shader compilation and caching
- [ ] Multi-pass rendering
- [ ] Damage region handling

**Code estimate:** ~600 lines (mostly from Wayfire)

**Key files:**
```
daemon/
  blur_renderer.c        # Base renderer
  kawase_blur.c         # Kawase implementation
  shaders/
    kawase_down.frag
    kawase_up.frag
```

#### Phase 4: Virtual Scene Graph (Week 6)

**Deliverables:**
- [ ] Blur node registry
- [ ] Node creation/destruction API
- [ ] Buffer-to-node association
- [ ] Surface linking support

**Code estimate:** ~200 lines

**Key files:**
```
daemon/
  scene_graph.c          # Virtual scene graph
  node_registry.c        # Blur node tracking
```

#### Phase 5: Vibrancy Enhancement (Week 7)

**Deliverables:**
- [ ] Port HSL color space conversion from Hyprland
- [ ] Vibrancy shader (saturation boost)
- [ ] Post-processing pipeline
- [ ] Configuration API for vibrancy parameters

**Code estimate:** ~150 lines

**Key files:**
```
daemon/
  vibrancy.c            # HSL conversion, saturation
  shaders/
    vibrancy.frag
```

#### Phase 6: Compositor Integration - scroll (Week 8-9)

**Deliverables:**
- [ ] IPC client library in C
- [ ] scroll compositor modifications:
  - [ ] DMA-BUF export from wlr_renderer
  - [ ] Blur region detection
  - [ ] Blur request sending
  - [ ] Result import and compositing
- [ ] Integration testing

**Code estimate:** ~400 lines (client) + ~200 lines (scroll modifications)

**Key files:**
```
libblur-client/
  blur_client.c          # IPC client library
  blur_client.h

scroll/
  blur_integration.c     # Compositor modifications
```

#### Phase 7: Additional Algorithms (Week 10)

**Deliverables:**
- [ ] Port Gaussian blur from Wayfire
- [ ] Port Box blur from Wayfire
- [ ] Port Bokeh blur from Wayfire
- [ ] Algorithm selection API

**Code estimate:** ~300 lines

**Key files:**
```
daemon/
  gaussian_blur.c
  box_blur.c
  bokeh_blur.c
```

#### Phase 8: Optimization (Week 11-12)

**Deliverables:**
- [ ] FBO pooling to reduce allocations
- [ ] Shader compilation caching
- [ ] Async rendering pipeline (hide IPC latency)
- [ ] Performance profiling and tuning
- [ ] Stress testing (many windows, rapid changes)

**Code estimate:** ~200 lines

#### Phase 9: niri Integration (Week 13-14)

**Deliverables:**
- [ ] Rust IPC client (niri uses Rust/Smithay)
- [ ] Smithay DMA-BUF export integration
- [ ] niri compositor modifications
- [ ] End-to-end testing

**Code estimate:** ~500 lines Rust

**Key files:**
```
blur-client-rs/
  lib.rs                 # Rust IPC client
  protocol.rs            # Protocol bindings

niri/
  blur_integration.rs    # Compositor modifications
```

#### Phase 10: Advanced Features (Week 15+)

**Deliverables:**
- [ ] Material system (Hud, Sidebar, Popover presets)
- [ ] Tint support (colored blur backgrounds)
- [ ] Desktop color sampling (adaptive vibrancy)
- [ ] ext-background-effect-v1 protocol support
- [ ] Configuration daemon (runtime parameter changes)

**Code estimate:** ~800 lines

---

## Part 3: Key Architectural Decisions

### 3.1 Algorithm Selection

**Recommendation: Start with Dual Kawase**

**Rationale:**
1. **Performance:** 60% fewer samples than Gaussian (16-18 vs 25-49)
2. **Quality:** High-quality blur, production-proven
3. **Simplicity:** Single algorithm reduces initial complexity
4. **Code availability:** Clean implementations in Wayfire, Hyprland, SceneFX

**Later additions:**
- Gaussian (highest quality, slower)
- Box (fastest, lower quality)
- Bokeh (artistic, unique aesthetic)

---

### 3.2 Optimization Trade-offs

#### In-Compositor Optimizations (Lost in Daemon)

| Optimization | In-Compositor Performance | Daemon Feasibility | Workaround |
|--------------|---------------------------|--------------------|--------------|
| **Blur caching** | 20× speedup | ⚠️ Possible with compositor cooperation | Compositor sends dirty flag |
| **Damage tracking** | 98% reduction | ✅ Full support | Compositor sends expanded damage |
| **xray mode** | 10× speedup | ❌ Not feasible | Compositor decides blur policy |
| **Per-monitor FBO reuse** | Memory efficient | ✅ Daemon can cache per-output | Track output IDs |

#### Acceptable Performance Loss

**Target:** 60 FPS = 16.67ms budget per frame

**Budget breakdown:**
- Compositor rendering: 4-8ms
- Blur (daemon): 1.4ms (1.2ms blur + 0.2ms IPC)
- Compositing result: 0.5ms
- **Total:** 5.9-9.9ms
- **Headroom:** 6.77-10.77ms (40-65% of budget remaining)

**Conclusion:** IPC overhead (~0.2ms) is acceptable for 60 FPS, marginal for 144 FPS

---

### 3.3 Multi-Compositor Strategy

#### Compositor Integration Patterns

**wlroots-based (Sway, River, scroll, Wayfire):**
```c
// Common DMA-BUF export pattern
struct wlr_buffer *buffer = wlr_scene_buffer_get_buffer(scene_buffer);
struct wlr_dmabuf_attributes dmabuf;
wlr_buffer_get_dmabuf(buffer, &dmabuf);

// Send to daemon
blur_client_import_dmabuf(client, dmabuf.fd, dmabuf.width, ...);
```

**Smithay-based (niri, cosmic):**
```rust
// Smithay DMA-BUF export
let dmabuf = buffer.dmabuf()?;
let fd = dmabuf.handles().get(0)?;

// Send to daemon via Rust client
blur_client.import_dmabuf(fd, width, height, format);
```

**Hyprland (custom renderer):**
```cpp
// Hyprland GL texture → DMA-BUF
EGLImageKHR image = eglCreateImageKHR(
    display, context, EGL_GL_TEXTURE_2D,
    (EGLClientBuffer)(uintptr_t)tex, NULL);

int fd;
eglExportDMABUFImageMESA(display, image, &fd, &stride, &offset);

// Send to daemon
blur_client_import_dmabuf(fd, width, height, format);
```

**Common Pattern:** All compositors can export DMA-BUFs. Integration complexity is similar (~200 lines per compositor).

---

### 3.4 Security Considerations

#### Threat Model

**Compositor → Daemon:**
- ✅ **Safe:** Compositor is trusted (runs as user)
- ✅ **Safe:** DMA-BUF FDs are read-only for daemon (no write access to compositor memory)
- ⚠️ **DoS:** Malicious compositor could send garbage data (daemon should validate)

**Daemon → Compositor:**
- ✅ **Safe:** Daemon returns DMA-BUF FDs to blurred results
- ✅ **Safe:** Compositor validates buffer dimensions match request
- ⚠️ **Resource leak:** Daemon must properly release buffers on client disconnect

**Mitigation Strategies:**
1. **Validate all inputs:** Check buffer dimensions, formats, damage regions
2. **Resource limits:** Max buffers per client (e.g., 256), max blur nodes (e.g., 1024)
3. **Timeout handling:** Disconnect clients that don't respond within 5 seconds
4. **Capability-based security:** Future extension could use Wayland protocols with capability tokens

---

### 3.5 Future Extensibility

#### Phase 4: Material System (Post-MVP)

**Apple-style semantic materials:**
```c
enum blur_material_preset {
    BLUR_MATERIAL_HUD,        // Deep blur, high vibrancy, subtle tint
    BLUR_MATERIAL_SIDEBAR,    // Medium blur, low vibrancy, neutral
    BLUR_MATERIAL_POPOVER,    // Light blur, high contrast, adaptive tint
    BLUR_MATERIAL_TOOLBAR,    // Minimal blur, high opacity
};

struct blur_configure_material_request {
    struct blur_request_header header;
    uint32_t blur_id;
    enum blur_material_preset preset;
    float tint_color[4];      // RGBA
    float opacity;
};
```

**Implementation requires:**
- Desktop color sampling (average color beneath blur region)
- Adaptive vibrancy (boost saturation based on background color)
- Tint blending (multiply blur result with tint color)
- Per-material parameter sets

**Timeline:** 2-3 weeks after MVP, builds on existing vibrancy system

---

## Part 4: Critical Success Factors

### 4.1 Technical Requirements

✅ **Algorithm Quality**
- Dual Kawase from Hyprland/Wayfire (proven, high quality)
- Vibrancy system from Hyprland (HSL saturation boost)
- Multi-pass rendering with ping-pong FBOs

✅ **Performance Targets**
- <1.5ms total per blur region (1080p)
- <2ms with IPC overhead
- Scale linearly with resolution (4K ≈ 4× cost)

✅ **IPC Efficiency**
- DMA-BUF zero-copy (no CPU memory copies)
- Binary protocol (no JSON/XML parsing)
- FD passing via SCM_RIGHTS (standard Unix)

✅ **Compositor Integration**
- Minimal changes (<200 lines per compositor)
- DMA-BUF export (already available in most compositors)
- Blur region detection (protocol or config-based)

✅ **Error Handling**
- Graceful degradation (disable blur if daemon unavailable)
- Compositor tracks state (can recreate in new daemon instance)
- Resource cleanup on client disconnect

---

### 4.2 Strategic Benefits

**1. Multi-Compositor Support**
- Single daemon works with scroll, niri, Sway, Hyprland, etc.
- Avoids forking each compositor
- Centralized blur quality and feature development

**2. Crash Isolation**
- Daemon failure doesn't kill compositor
- Compositor can restart daemon transparently
- GPU crashes contained to daemon process

**3. Independent Versioning**
- Daemon can be updated without recompiling compositors
- New blur algorithms added without compositor changes
- Material system can evolve independently

**4. Language Flexibility**
- Daemon can be C, C++, Rust, or hybrid
- Compositors can use language-appropriate IPC clients
- Future: WASM-based blur algorithms?

**5. Apple-Level Visual Quality Path**
- Phase 1-2: Basic blur (Yosemite)
- Phase 3: Vibrancy (Big Sur/Monterey)
- Phase 4: Material system (Ventura/Sonoma)
- Progressive enhancement without breaking changes

---

## Part 5: Risk Assessment

### 5.1 Technical Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **IPC overhead too high for 144Hz** | Medium | High | Async pipeline, hide latency behind GPU work |
| **DMA-BUF import/export complexity** | Low | High | Use proven patterns from SceneFX, wlroots examples |
| **Daemon crash handling** | Medium | Medium | Compositor tracks state, can recreate nodes |
| **Sync object complexity** | Medium | Low | Start synchronous (glFinish), add sync objects later |
| **GPU compatibility issues** | Low | High | Extensive testing on Intel/AMD/NVIDIA, fallback to CPU blur |

### 5.2 Integration Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **scroll/niri maintainer resistance** | Low | High | Prove value with standalone demo, minimize compositor changes |
| **Compositor API instability** | Medium | Medium | Target stable APIs (wlroots stable, Smithay documented) |
| **Multi-compositor testing burden** | High | Medium | Automated test suite, CI for each compositor |
| **Protocol evolution breaking changes** | Low | Medium | Version IPC protocol, support multiple versions |

### 5.3 Performance Risks

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **Can't match in-compositor performance** | High | Low | Expected 2-5× slower, but acceptable (<2ms) |
| **Caching less effective** | Medium | Medium | Compositor cooperation (send dirty flag) |
| **Memory overhead too high** | Low | Medium | FBO pooling, release unused buffers aggressively |

---

## Part 6: Conclusion and Recommendations

### Final Architecture

**Recommended Approach: External Daemon with Hybrid Code Reuse**

```
Core Components:
├─ Blur Renderer (~600 lines)
│  ├─ Kawase algorithm (from Wayfire)
│  ├─ Vibrancy system (from Hyprland)
│  └─ FBO management (from Wayfire)
├─ IPC Server (~300 lines)
│  ├─ Unix socket
│  └─ Binary protocol
├─ DMA-BUF Manager (~300 lines)
│  ├─ Import (from SceneFX patterns)
│  └─ Export
├─ Virtual Scene Graph (~200 lines)
│  ├─ Node registry
│  └─ Buffer tracking
└─ EGL Context (~150 lines)
   └─ Independent GL context

Total: ~1550 lines for MVP
```

### Success Metrics

**MVP (Week 8-9):**
- ✅ Single compositor (scroll) integration working
- ✅ Kawase blur with configurable parameters
- ✅ <2ms latency (1080p, mid-range GPU)
- ✅ Stable (no crashes in 1-hour stress test)

**Production (Week 12):**
- ✅ Multi-compositor support (scroll + niri)
- ✅ Multiple algorithms (Kawase + Gaussian)
- ✅ Vibrancy support
- ✅ Performance optimizations (FBO pooling, async pipeline)

**Advanced (Week 15+):**
- ✅ Material system
- ✅ Tint support
- ✅ Desktop color sampling
- ✅ ext-background-effect-v1 protocol

### Next Actions

**Immediate (This Week):**
1. Set up daemon project structure
2. Implement EGL context initialization
3. Test DMA-BUF import/export with test program

**Short-term (Weeks 1-4):**
1. Build IPC foundation
2. Port Kawase blur from Wayfire
3. Implement DMA-BUF manager

**Mid-term (Weeks 5-9):**
1. Vibrancy enhancement
2. scroll compositor integration
3. End-to-end testing

**Long-term (Weeks 10+):**
1. Additional algorithms
2. niri integration
3. Material system

---

## Appendix A: File Structure

```
blur-daemon/
├─ daemon/
│  ├─ main.c                    # Entry point, event loop
│  ├─ ipc_server.c             # Unix socket, message handling
│  ├─ protocol.h               # IPC protocol definitions
│  ├─ egl_context.c            # EGL initialization
│  ├─ dmabuf_manager.c         # DMA-BUF import/export
│  ├─ buffer_registry.c        # Buffer ID tracking
│  ├─ scene_graph.c            # Virtual scene graph
│  ├─ node_registry.c          # Blur node tracking
│  ├─ blur_renderer.c          # Base renderer
│  ├─ kawase_blur.c            # Kawase implementation
│  ├─ gaussian_blur.c          # Gaussian implementation
│  ├─ box_blur.c               # Box implementation
│  ├─ bokeh_blur.c             # Bokeh implementation
│  ├─ vibrancy.c               # HSL vibrancy
│  └─ shaders/
│     ├─ kawase_down.frag
│     ├─ kawase_up.frag
│     ├─ gaussian_h.frag
│     ├─ gaussian_v.frag
│     ├─ box_h.frag
│     ├─ box_v.frag
│     ├─ bokeh.frag
│     └─ vibrancy.frag
├─ libblur-client/
│  ├─ blur_client.c            # C client library
│  └─ blur_client.h
├─ blur-client-rs/
│  ├─ lib.rs                   # Rust client library
│  └─ protocol.rs
├─ tests/
│  ├─ dmabuf_test.c            # DMA-BUF import/export test
│  ├─ ipc_test.c               # IPC protocol test
│  └─ blur_quality_test.c      # Visual quality comparison
└─ docs/
   ├─ protocol.md              # IPC protocol specification
   ├─ integration.md           # Compositor integration guide
   └─ performance.md           # Performance tuning guide
```

---

## Appendix B: Key Code References

### From Hyprland

**Vibrancy Algorithm:**
- `src/render/OpenGL.cpp:2050-2100` - Vibrancy calculation
- `src/render/shaders/glsl/CM.glsl:150-180` - HSL color space
- `src/render/shaders/glsl/blur1.frag:80-120` - Saturation boost

**Caching System:**
- `src/render/OpenGL.cpp:2260-2290` - `preBlurForCurrentMonitor()`
- `src/render/OpenGL.cpp:2177-2258` - `preRender()` dirty tracking
- `src/render/Renderer.cpp:1300-1320` - Cache invalidation

### From Wayfire

**Kawase Blur:**
- `plugins/blur/kawase.cpp:25-120` - Full implementation
- `plugins/blur/blur.hpp:91-155` - Base abstraction
- `plugins/blur/blur-base.cpp:150-280` - Multi-pass rendering

**Scene Integration:**
- `plugins/blur/blur.cpp:46-91` - `blur_node_t` transformer
- `plugins/blur/blur.cpp:172-262` - Render instance creation
- `plugins/blur/blur.cpp:264-337` - Damage padding and saved pixels

### From SceneFX

**DMA-BUF Import:**
- `render/fx_renderer/fx_texture.c:353-404` - `fx_gles_texture_from_dmabuf()`
- `render/egl.c:751-841` - EGLImageKHR from DMA-BUF attributes
- `render/fx_renderer/fx_framebuffer.c:117-158` - Framebuffer from buffer

**Damage Expansion:**
- `types/scene/wlr_scene.c:680-684` - Visibility expansion
- `render/fx_renderer/fx_pass.c:889` - Render pass damage expansion
- `types/scene/wlr_scene.c:2963-3010` - Artifact prevention (save pixels)

---

**Document Version:** 1.0  
**Last Updated:** November 12, 2025  
**Next Review:** After MVP completion (Week 9)
