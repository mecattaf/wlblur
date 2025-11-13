# SceneFX Architecture Investigation

**Date:** November 12, 2025
**Repository:** https://github.com/wlrfx/scenefx
**Purpose:** Understanding scene graph replacement strategy for blur compositor integration

---

## Overview

SceneFX is a **drop-in replacement for wlroots' scene graph** that extends the rendering capabilities with visual effects (blur, shadows, rounded corners). It maintains API compatibility with wlroots while adding an FX renderer layer that enables multi-pass rendering.

**Key Architectural Principle:** Extend, don't replace. SceneFX wraps wlroots structures and extends them with additional FX-specific fields while maintaining binary compatibility with existing wlroots APIs.

---

## Scene Graph Replacement Strategy

### wlroots Scene Graph (Baseline)

```c
struct wlr_scene {
    struct wlr_scene_tree tree;
    struct wl_list outputs;
    // ... standard wlroots fields ...
};

struct wlr_scene_node {
    enum wlr_scene_node_type type;
    struct wlr_scene_tree *parent;
    struct wl_list link;
    bool enabled;
    int x, y;
    // ... standard fields ...
};
```

### SceneFX Extensions

**File:** `include/scenefx/types/wlr_scene.h`

```c
// Extended node types
enum wlr_scene_node_type {
    WLR_SCENE_NODE_TREE,
    WLR_SCENE_NODE_RECT,
    WLR_SCENE_NODE_SHADOW,         // NEW: Drop shadows
    WLR_SCENE_NODE_BUFFER,
    WLR_SCENE_NODE_OPTIMIZED_BLUR, // NEW: Cached blur for static content
    WLR_SCENE_NODE_BLUR,           // NEW: Dynamic blur regions
};

// Extended scene structure
struct wlr_scene {
    struct wlr_scene_tree tree;
    struct wl_list outputs;
    struct wlr_linux_dmabuf_v1 *linux_dmabuf_v1;

    struct {
        // ... wlroots private fields ...
        struct blur_data blur_data;  // NEW: Global blur parameters
    } WLR_PRIVATE;
};
```

**Strategy:** Add new node types to existing enum, extend structures with private fields, maintain same memory layout for base structures.

---

## API Compatibility Layer

### Function Wrapping Pattern

SceneFX provides all standard wlroots scene functions while adding FX variants:

```c
// Standard wlroots API (maintained)
struct wlr_scene_surface *wlr_scene_surface_create(
    struct wlr_scene_tree *parent,
    struct wlr_surface *surface
);

// SceneFX extensions (added)
void wlr_scene_buffer_set_corner_radius(
    struct wlr_scene_buffer *buffer,
    int radii,
    enum corner_location corners  // NEW parameter type
);

struct wlr_scene_blur *wlr_scene_blur_create(
    struct wlr_scene_tree *parent,
    int width, int height
);
```

**Key Insight:** Existing compositors can compile against SceneFX without changes. New effects are opt-in.

---

## FX Renderer Integration

### Renderer Hierarchy

```
wlr_renderer (interface)
    ↓ implements
fx_renderer (implementation)
    ↓ contains
struct fx_renderer {
    struct wlr_renderer wlr_renderer;  // Base renderer (first field = compatible casting)
    struct wlr_egl *egl;

    struct {
        struct quad_shader quad;
        struct tex_shader tex_rgba;
        struct blur_shader blur1;       // Downsample shader
        struct blur_shader blur2;       // Upsample shader
        struct blur_effects_shader blur_effects;  // Post-processing
        struct box_shadow_shader box_shadow;
        // ... more shaders ...
    } shaders;

    struct wl_list buffers;        // fx_framebuffer list
    struct wl_list textures;       // fx_texture list
    struct wl_list effect_fbos;    // Per-output effect framebuffers
};
```

**File:** `include/render/fx_renderer/fx_renderer.h` (lines 141-198)

### Render Pass Extensions

SceneFX extends `wlr_render_pass` with FX-specific operations:

```c
// Standard wlroots interface
struct wlr_render_pass {
    const struct wlr_render_pass_impl *impl;
};

static const struct wlr_render_pass_impl render_pass_impl = {
    .submit = render_pass_submit,
    .add_texture = render_pass_add_texture,  // Wraps FX version with defaults
    .add_rect = render_pass_add_rect,        // Wraps FX version with defaults
};

// FX extensions
void fx_render_pass_add_texture(struct fx_gles_render_pass *pass,
    const struct fx_render_texture_options *fx_options);

void fx_render_pass_add_rect(struct fx_gles_render_pass *pass,
    const struct fx_render_rect_options *fx_options);

void fx_render_pass_add_blur(struct fx_gles_render_pass *pass,
    const struct fx_render_blur_pass_options *options);
```

**File:** `render/fx_renderer/fx_pass.c` (lines 147-151)

---

## Scene Graph Extension Pattern

### Base Node Structure

All scene nodes inherit from `wlr_scene_node`:

```c
struct wlr_scene_node {
    enum wlr_scene_node_type type;  // Discriminator for casting
    struct wlr_scene_tree *parent;
    struct wl_list link;
    bool enabled;
    int x, y;
    struct wl_signal events.destroy;
    void *data;  // User data
    struct wlr_addon_set addons;

    struct {
        pixman_region32_t visible;  // Computed visibility region
    } WLR_PRIVATE;
};
```

### Effect Node Extensions

**Blur Node:**
```c
struct wlr_scene_blur {
    struct wlr_scene_node node;  // Inheritance (first field)

    int width, height;
    int corner_radius;
    enum corner_location corners;
    struct clipped_region clipped_region;

    float strength;  // 0.0-1.0: blur intensity scaling
    float alpha;     // 0.0-1.0: blur opacity

    bool should_only_blur_bottom_layer;  // Don't blur windows above

    struct linked_node transparency_mask_source;  // Link to surface buffer
};
```

**File:** `include/scenefx/types/wlr_scene.h` (lines 170-184)

**Shadow Node:**
```c
struct wlr_scene_shadow {
    struct wlr_scene_node node;

    int width, height;
    int corner_radius;
    float color[4];
    float blur_sigma;  // Gaussian blur for shadow

    struct clipped_region clipped_region;
};
```

**File:** `include/scenefx/types/wlr_scene.h` (lines 160-168)

**Extended Buffer Node:**
```c
struct wlr_scene_buffer {
    struct wlr_scene_node node;
    struct wlr_buffer *buffer;

    // Standard wlroots fields
    struct wlr_scene_output *primary_output;
    int dst_width, dst_height;
    enum wl_output_transform transform;
    pixman_region32_t opaque_region;

    // SceneFX extensions
    int corner_radius;              // NEW
    enum corner_location corners;   // NEW
    float opacity;                  // NEW
    enum wlr_scale_filter_mode filter_mode;  // NEW
    struct linked_node blur;        // NEW: Link to blur node
};
```

**File:** `include/scenefx/types/wlr_scene.h` (lines 205-263)

---

## Multi-Pass Rendering Architecture

### Rendering Pipeline Flow

```
┌─────────────────────────────────────────────────────────────────┐
│  Scene Graph Traversal (Bottom-to-Top)                          │
│  - Compute visibility regions                                    │
│  - Build render list                                             │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Base Layer Rendering                                            │
│  - Render background                                             │
│  - Render non-effect nodes                                       │
│  - Output to: effects_buffer                                     │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Blur Processing (if blur nodes present)                         │
│                                                                   │
│  FOR EACH blur node:                                             │
│    1. Expand damage region by blur_size                          │
│    2. Downsample passes (blur1 shader):                          │
│       - Pass 1: Full res → Half res                              │
│       - Pass 2: Half res → Quarter res                           │
│       - Pass N: 2^-(N) scale                                     │
│       - Ping-pong between effects_buffer ↔ effects_buffer_swapped│
│    3. Upsample passes (blur2 shader):                            │
│       - Reverse order back to full res                           │
│       - Weighted sampling for smooth blur                        │
│    4. Apply post-effects (blur_effects shader):                  │
│       - Brightness adjustment                                    │
│       - Contrast adjustment                                      │
│       - Saturation adjustment                                    │
│       - Noise overlay                                            │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Shadow Rendering (if shadow nodes present)                      │
│  - Use box_shadow shader                                         │
│  - Gaussian blur based on blur_sigma                             │
└─────────────────────────────────────────────────────────────────┘
                            ↓
┌─────────────────────────────────────────────────────────────────┐
│  Final Composition                                               │
│  - Composite all layers                                          │
│  - Apply corner radius masks                                     │
│  - Restore blur artifact padding pixels                          │
│  - Output to: output framebuffer                                 │
└─────────────────────────────────────────────────────────────────┘
```

### Per-Output Effect Framebuffers

Each output maintains a set of effect framebuffers for multi-pass rendering:

```c
struct fx_effect_framebuffers {
    struct wl_list link;           // Linked into fx_renderer.effect_fbos
    struct wlr_addon addon;        // Attached to wlr_output lifecycle

    // Pre-computed blur for static content (wallpapers)
    struct fx_framebuffer *optimized_blur_buffer;
    struct fx_framebuffer *optimized_no_blur_buffer;

    // Artifact prevention: saves original pixels at blur edges
    struct fx_framebuffer *blur_saved_pixels_buffer;

    // Ping-pong buffers for multi-pass effects
    struct fx_framebuffer *effects_buffer;
    struct fx_framebuffer *effects_buffer_swapped;

    // Damage tracking for blur
    pixman_region32_t blur_padding_region;
};
```

**File:** `include/scenefx/render/fx_renderer/fx_effect_framebuffers.h` (lines 11-30)

**Lifecycle:**
- Created lazily on first use via `fx_effect_framebuffers_try_get(output)`
- Attached to `wlr_output` via addon system
- Automatically destroyed when output is destroyed
- Recreated if output is resized

---

## Hook Points for Custom Logic

### Scene Rendering Hook

**File:** `types/scene/wlr_scene.c` (lines 2050-2120)

```c
static void scene_entry_render(struct render_list_entry *entry,
    struct render_data *data) {

    struct wlr_scene_node *node = entry->node;

    switch (node->type) {
    case WLR_SCENE_NODE_TREE:
        // No rendering, just a container
        break;

    case WLR_SCENE_NODE_RECT:
        // Render solid rectangle with optional rounded corners
        render_rect(data, entry);
        break;

    case WLR_SCENE_NODE_SHADOW:
        // Render drop shadow with blur
        render_shadow(data, entry);
        break;

    case WLR_SCENE_NODE_BUFFER:
        // Render texture/surface with effects
        render_buffer(data, entry);
        break;

    case WLR_SCENE_NODE_OPTIMIZED_BLUR:
        // Render cached blur (wallpaper blur)
        render_optimized_blur(data, entry);
        break;

    case WLR_SCENE_NODE_BLUR:
        // Render dynamic blur region
        render_blur(data, entry);
        break;
    }
}
```

**Integration Point:** Compositors can extend this by adding new node types or hooking render callbacks.

---

## Addon System for Resource Management

SceneFX uses wlroots' addon system to attach resources to objects:

```c
// Attach fx_framebuffer to wlr_buffer
struct fx_framebuffer {
    struct wlr_buffer *buffer;
    struct wlr_addon addon;  // Lifecycle tied to buffer
    // ... GL resources ...
};

// When buffer is destroyed, addon callback is invoked:
static void buffer_addon_destroy(struct wlr_addon *addon) {
    struct fx_framebuffer *buffer = wl_container_of(addon, buffer, addon);
    // Clean up GL resources
    fx_framebuffer_destroy(buffer);
}

// Registration:
wlr_addon_init(&buffer->addon, &wlr_buffer->addons, renderer, &buffer_addon_impl);
```

**File:** `render/fx_renderer/fx_framebuffer.c` (lines 70-90, 143-148)

**Benefits:**
- Automatic cleanup when parent object is destroyed
- Per-renderer isolation (multiple renderers can attach different addons)
- Efficient lookup: O(1) check if addon exists

---

## API Compatibility Summary

### What SceneFX Maintains

✅ All standard wlroots scene functions work unchanged
✅ Binary compatibility for `wlr_renderer` interface
✅ Existing compositor code compiles without modification
✅ Same memory layout for base structures
✅ Standard wlroots protocols work (dmabuf, presentation, etc.)

### What SceneFX Extends

➕ New scene node types (blur, shadow, optimized_blur)
➕ Extended buffer/rect properties (corner radius, opacity)
➕ Global blur configuration API
➕ Per-surface effect control
➕ Multi-pass rendering pipeline

### What Breaks Compatibility

❌ **Requires GLES2/GLES3:** Won't work with Vulkan-only renderers
❌ **Statically linked wlroots:** Must fork wlroots to integrate
❌ **API is unstable:** Subject to change, no stability guarantees

---

## Integration Pattern for scroll/niri

### For Scroll (wlroots-based)

**Option 1: Fork SceneFX (Recommended)**
```bash
# Scroll already forks wlroots
# Can merge SceneFX changes into scroll's wlroots fork
git remote add scenefx https://github.com/wlrfx/scenefx
git fetch scenefx
git merge scenefx/main
# Resolve conflicts with scroll's custom scene changes
```

**Option 2: External Blur Daemon**
- Import SceneFX blur shaders and rendering logic into standalone daemon
- Compositor exports DMA-BUF textures to daemon
- Daemon returns blurred textures
- Compositor composites final result
- **Advantage:** No wlroots fork needed
- **Disadvantage:** IPC overhead, more complex state management

### For Niri (Smithay-based)

**Must use External Blur Daemon approach:**
- Niri uses Rust/Smithay, can't directly integrate C code
- Daemon can be language-agnostic (C, Rust, or hybrid)
- Reuse SceneFX blur algorithms in daemon
- Similar DMA-BUF import/export pattern

---

## Key Takeaways

1. **SceneFX achieves compatibility through careful struct extension** - Base structures unchanged, new fields in private sections
2. **Multi-pass rendering requires dedicated framebuffers** - 5 FBOs per output
3. **Damage tracking must expand regions** - Blur size = 2^(passes+1) × radius
4. **Addon system enables clean resource management** - Automatic cleanup, no manual tracking
5. **Scene graph is the source of truth** - All effects are scene nodes, rendered in tree order

**For External Daemon:** Need to replicate scene graph structure, framebuffer management, and damage tracking in IPC layer.
