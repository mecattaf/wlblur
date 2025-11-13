# SceneFX API Compatibility Analysis

**Investigation Date:** November 12, 2025
**Purpose:** Document wlroots API compatibility patterns for scroll integration

---

## API Compatibility Strategy

### Core Principle: Struct Extension via Inheritance

SceneFX maintains binary compatibility with wlroots by **placing the base struct as the first field** in extended structs. This allows safe casting between types.

**Example:**
```c
// wlroots base
struct wlr_renderer {
    const struct wlr_renderer_impl *impl;
    // ... other wlroots fields ...
};

// SceneFX extension
struct fx_renderer {
    struct wlr_renderer wlr_renderer;  // FIRST FIELD = compatible cast
    struct wlr_egl *egl;
    // ... FX-specific fields ...
};

// Safe casting
struct wlr_renderer *wlr_renderer = &fx_renderer->wlr_renderer;
```

**Memory layout:**
```
fx_renderer:
┌─────────────────────────────────┐
│ wlr_renderer (base)             │ ← ptr to fx_renderer also points here
│  - impl                          │
│  - ...                           │
├─────────────────────────────────┤
│ egl                              │
│ shaders                          │
│ buffers                          │
│ ...                              │
└─────────────────────────────────┘
```

---

## Function Signature Compatibility

### Pattern 1: Direct Replacement

**wlroots original:**
```c
struct wlr_scene *wlr_scene_create(void);
```

**SceneFX replacement:**
```c
struct wlr_scene *wlr_scene_create(void);
```

**Change:** None (identical signature)
**Compatibility:** ✅ Perfect

---

### Pattern 2: Extended Parameters

**wlroots original:**
```c
struct wlr_scene_rect *wlr_scene_rect_create(
    struct wlr_scene_tree *parent,
    int width, int height,
    const float color[static 4]
);
```

**SceneFX addition:**
```c
void wlr_scene_rect_set_corner_radius(
    struct wlr_scene_rect *rect,
    int corner_radius,
    enum corner_location corners  // NEW type
);
```

**Change:** New function added, original unchanged
**Compatibility:** ✅ Perfect (opt-in feature)

---

### Pattern 3: Struct Field Extension

**wlroots base:**
```c
struct wlr_scene_node {
    enum wlr_scene_node_type type;
    struct wlr_scene_tree *parent;
    struct wl_list link;
    bool enabled;
    int x, y;
    // ...
};
```

**SceneFX extension (compatible):**
```c
struct wlr_scene_node {
    enum wlr_scene_node_type type;  // Extended enum
    struct wlr_scene_tree *parent;
    struct wl_list link;
    bool enabled;
    int x, y;
    // ... same fields ...

    struct {
        pixman_region32_t visible;  // FX-specific
    } WLR_PRIVATE;  // Marked as private (unstable)
};
```

**Change:** Enum extended, private fields added
**Compatibility:** ✅ Binary compatible (same offsets for base fields)

---

## Enum Extension Pattern

### Safe Enum Extension

**wlroots original:**
```c
enum wlr_scene_node_type {
    WLR_SCENE_NODE_TREE,
    WLR_SCENE_NODE_RECT,
    WLR_SCENE_NODE_BUFFER,
};
```

**SceneFX extension:**
```c
enum wlr_scene_node_type {
    WLR_SCENE_NODE_TREE,
    WLR_SCENE_NODE_RECT,
    WLR_SCENE_NODE_SHADOW,         // NEW
    WLR_SCENE_NODE_BUFFER,
    WLR_SCENE_NODE_OPTIMIZED_BLUR, // NEW
    WLR_SCENE_NODE_BLUR,           // NEW
};
```

**Impact:**
- Existing code checking for specific types: ✅ Still works
- Existing code with switch statements: ⚠️ May hit default case for new types
- Existing code iterating nodes: ✅ Still works (skips unknown types)

**Compatibility:** ⚠️ Mostly compatible (requires compositor awareness of new types)

---

## Renderer API Compatibility

### wlroots Renderer Interface

**File:** `wlr/render/interface.h`

```c
struct wlr_renderer_impl {
    const struct wlr_render_pass_impl *(*begin_buffer_pass)(
        struct wlr_renderer *renderer,
        struct wlr_buffer *buffer,
        const struct wlr_buffer_pass_options *options
    );

    struct wlr_render_timer *(*render_timer_create)(
        struct wlr_renderer *renderer
    );

    // ... more functions ...
};
```

### SceneFX Implementation

**File:** `render/fx_renderer/fx_renderer.c` (lines 518-531)

```c
static const struct wlr_renderer_impl renderer_impl = {
    .begin_buffer_pass = fx_renderer_begin_buffer_pass,  // FX version
    .render_timer_create = fx_render_timer_create,
    .get_buffer_caps = fx_get_buffer_caps,
    .get_render_formats = fx_get_render_formats,
    // ... implements all required functions ...
};

struct wlr_renderer *fx_renderer_create_egl(struct wlr_egl *egl) {
    struct fx_renderer *renderer = calloc(1, sizeof(*renderer));

    // Initialize base wlr_renderer with FX impl
    wlr_renderer_init(&renderer->wlr_renderer, &renderer_impl);

    // ... initialize FX-specific fields ...
    return &renderer->wlr_renderer;
}
```

**Compatibility:** ✅ Perfect (implements full wlr_renderer interface)

---

## Render Pass Extension

### Two-Tier API

**Standard wlroots pass:**
```c
struct wlr_render_pass {
    const struct wlr_render_pass_impl *impl;
};

static const struct wlr_render_pass_impl render_pass_impl = {
    .submit = render_pass_submit,
    .add_texture = render_pass_add_texture,  // Wrapper
    .add_rect = render_pass_add_rect,        // Wrapper
};
```

**FX-specific pass (extended):**
```c
struct fx_gles_render_pass {
    struct wlr_render_pass base;  // FIRST FIELD
    struct fx_framebuffer *buffer;
    struct fx_effect_framebuffers *fx_effect_framebuffers;
    // ... FX fields ...
};

// FX-specific functions (not in wlr_render_pass_impl)
void fx_render_pass_add_texture(struct fx_gles_render_pass *pass,
    const struct fx_render_texture_options *options);

void fx_render_pass_add_blur(struct fx_gles_render_pass *pass,
    const struct fx_render_blur_pass_options *options);
```

**Standard API wraps FX API:**
```c
static void render_pass_add_texture(struct wlr_render_pass *wlr_pass,
    const struct wlr_render_texture_options *options) {

    struct fx_gles_render_pass *pass = get_render_pass(wlr_pass);

    // Convert standard options to FX options (with defaults)
    const struct fx_render_texture_options fx_options =
        fx_render_texture_options_default(options);

    // Call FX implementation
    fx_render_pass_add_texture(pass, &fx_options);
}
```

**Compatibility:**
- ✅ Standard wlroots code works (calls wrapper)
- ✅ SceneFX-aware code gets FX features (calls FX functions directly)

---

## API Versioning and Stability

### wlroots Unstable API

**All SceneFX functions require:**
```c
#ifndef WLR_USE_UNSTABLE
#error "Add -DWLR_USE_UNSTABLE to enable unstable wlroots features"
#endif
```

**Implication:** SceneFX **inherits wlroots' instability guarantee**.

**File:** `include/scenefx/types/wlr_scene.h` (lines 2-7)

---

### SceneFX Version Compatibility

**SceneFX does not maintain API stability.**

**From README.md:**
> SceneFX is in active development. Breaking changes may occur.

**Implications for scroll integration:**
- ⚠️ May need to update scroll when updating SceneFX
- ⚠️ Cannot use system-packaged SceneFX (vendoring recommended)
- ✅ Can fork and freeze at specific SceneFX commit

---

## Function Mapping Table

| Category | wlroots Function | SceneFX Equivalent | Change |
|----------|------------------|---------------------|---------|
| **Scene Creation** | `wlr_scene_create()` | `wlr_scene_create()` | None |
| **Scene Tree** | `wlr_scene_tree_create()` | `wlr_scene_tree_create()` | None |
| **Scene Surface** | `wlr_scene_surface_create()` | `wlr_scene_surface_create()` | None |
| **Scene Buffer** | `wlr_scene_buffer_create()` | `wlr_scene_buffer_create()` | None |
| **Scene Rect** | `wlr_scene_rect_create()` | `wlr_scene_rect_create()` | None |
| **Rect Color** | `wlr_scene_rect_set_color()` | `wlr_scene_rect_set_color()` | None |
| **Rect Size** | `wlr_scene_rect_set_size()` | `wlr_scene_rect_set_size()` | None |
| **Rect Corners** | N/A | `wlr_scene_rect_set_corner_radius()` | **NEW** |
| **Buffer Opacity** | N/A | `wlr_scene_buffer_set_opacity()` | **NEW** |
| **Buffer Corners** | N/A | `wlr_scene_buffer_set_corner_radius()` | **NEW** |
| **Shadow** | N/A | `wlr_scene_shadow_create()` | **NEW** |
| **Blur** | N/A | `wlr_scene_blur_create()` | **NEW** |
| **Optimized Blur** | N/A | `wlr_scene_optimized_blur_create()` | **NEW** |
| **Blur Parameters** | N/A | `wlr_scene_set_blur_data()` | **NEW** |
| **Renderer Creation** | `wlr_renderer_autocreate()` | `fx_renderer_create()` | Changed (different function) |
| **Render Pass** | `wlr_renderer_begin_buffer_pass()` | `wlr_renderer_begin_buffer_pass()` | Same name, FX impl |

---

## Breaking Changes from wlroots

### 1. Renderer Creation

**wlroots:**
```c
struct wlr_renderer *renderer = wlr_renderer_autocreate(backend);
```

**SceneFX:**
```c
struct wlr_renderer *renderer = fx_renderer_create(backend);
// OR
struct wlr_renderer *renderer = fx_renderer_create_with_drm_fd(drm_fd);
```

**Reason:** Need to initialize FX-specific shader programs and extensions.

---

### 2. Scene Output Commit

**wlroots:**
```c
bool wlr_scene_output_commit(struct wlr_scene_output *scene_output,
    const struct wlr_scene_output_state_options *options);
```

**SceneFX:**
```c
bool wlr_scene_output_commit(struct wlr_scene_output *scene_output,
    const struct wlr_scene_output_state_options *options);
```

**Looks the same, but internal behavior differs:**
- SceneFX applies blur passes before commit
- SceneFX manages effect framebuffers
- SceneFX expands damage regions

**Compatibility:** ✅ API unchanged, behavior extended

---

### 3. Required wlroots Version

**SceneFX requirement:**
```
wlroots >= 0.18.0 (or recent git)
```

**Reason:**
- Uses `wlr_addon` system (added in 0.16)
- Uses `wlr_render_pass` API (refactored in 0.17)
- Uses `wlr_drm_syncobj_timeline` (added in 0.18)

**Implication:** scroll must use compatible wlroots version.

---

## Integration Patterns for scroll

### Pattern 1: Full SceneFX Integration (Recommended)

**Approach:**
- Fork SceneFX or vendor as submodule
- Replace `wlr_renderer_autocreate()` with `fx_renderer_create()`
- Use FX scene node types where desired
- Keep standard wlroots scene nodes elsewhere

**Pros:**
- ✅ Maximum performance (in-process)
- ✅ Full access to FX features
- ✅ No IPC overhead

**Cons:**
- ❌ Tightly coupled to SceneFX
- ❌ Must track SceneFX updates
- ❌ Conflicts with scroll's custom scene changes

**Code example:**
```c
// scroll compositor initialization
struct wlr_renderer *renderer = fx_renderer_create(backend);

// Use standard scene nodes
struct wlr_scene_buffer *wallpaper_buffer = wlr_scene_buffer_create(...);

// Add FX nodes
struct wlr_scene_optimized_blur *wallpaper_blur =
    wlr_scene_optimized_blur_create(parent, output_width, output_height);

struct wlr_scene_blur *window_blur = wlr_scene_blur_create(window_tree, 0, 0);
wlr_scene_blur_set_transparency_mask_source(window_blur, window_buffer);
```

---

### Pattern 2: External Daemon (Flexible)

**Approach:**
- scroll uses standard wlroots renderer
- Extract SceneFX blur shaders to external daemon
- Compositor sends DMA-BUFs to daemon via IPC
- Daemon returns blurred textures

**Pros:**
- ✅ No wlroots fork needed
- ✅ scroll remains close to upstream
- ✅ Can work with niri (Smithay) too
- ✅ Daemon can be language-agnostic

**Cons:**
- ❌ ~0.16ms IPC overhead per blur
- ❌ More complex state management
- ❌ Requires DMA-BUF + sync object support

**See:** `daemon-translation.md` for full implementation guide

---

### Pattern 3: Hybrid (Best of Both)

**Approach:**
- Use SceneFX for simple effects (rounded corners, shadows)
- Use external daemon for complex blur (vibrancy, tint, etc.)

**Pros:**
- ✅ Simple effects stay fast (in-process)
- ✅ Complex effects stay flexible (daemon)
- ✅ Gradual migration path

**Cons:**
- ❌ Two rendering paths to maintain
- ❌ Most complex to implement

---

## Compatibility Testing Strategy

### Test 1: Standard wlroots Code

```c
// This code should work unchanged with SceneFX
struct wlr_scene *scene = wlr_scene_create();
struct wlr_scene_output *scene_output = wlr_scene_output_create(scene, output);

struct wlr_scene_rect *rect = wlr_scene_rect_create(
    &scene->tree, 100, 100, (float[]){1.0, 0.0, 0.0, 1.0});

wlr_scene_output_commit(scene_output, NULL);
```

**Expected:** ✅ Works identically

---

### Test 2: FX-Specific Code

```c
// This code requires SceneFX
wlr_scene_set_blur_data(scene, 3, 5, 0.02, 0.9, 0.9, 1.1);

struct wlr_scene_blur *blur = wlr_scene_blur_create(&scene->tree, 800, 600);
wlr_scene_blur_set_strength(blur, 1.0);

wlr_scene_rect_set_corner_radius(rect, 10, CORNER_LOCATION_ALL);

wlr_scene_output_commit(scene_output, NULL);
```

**Expected:** ✅ Works only with SceneFX

---

### Test 3: Mixed Code

```c
// Mix standard and FX code
struct wlr_scene_tree *tree = wlr_scene_tree_create(&scene->tree);  // Standard

struct wlr_scene_rect *bg = wlr_scene_rect_create(
    tree, 100, 100, (float[]){0.5, 0.5, 0.5, 1.0});  // Standard

wlr_scene_rect_set_corner_radius(bg, 20, CORNER_LOCATION_ALL);  // FX

struct wlr_scene_blur *blur = wlr_scene_blur_create(tree, 100, 100);  // FX

wlr_scene_output_commit(scene_output, NULL);  // Standard
```

**Expected:** ✅ Works only with SceneFX

---

## Potential Conflicts with scroll's Custom Scene

### scroll's Scene Modifications

From issue #3 discussion, **scroll already modifies wlr_scene for content and workspace scaling**.

**Potential conflict areas:**
1. **Scene node traversal** - scroll may have custom traversal logic
2. **Damage tracking** - scroll may compute damage differently
3. **Rendering pipeline** - scroll may have custom render passes

**Resolution strategies:**

**Strategy 1: Merge scroll's changes into SceneFX**
- Fork SceneFX
- Port scroll's scaling features into SceneFX renderer
- Maintain single unified renderer

**Strategy 2: Keep separate, selective integration**
- Use SceneFX for effects only
- Use scroll's renderer for scaling
- Compositor decides which renderer to use per node type

**Strategy 3: External daemon (avoid conflict entirely)**
- scroll keeps its custom renderer
- Blur handled out-of-process
- Zero conflict

---

## API Compatibility Checklist

For scroll integration, verify:

- [ ] **wlroots version compatibility** - scroll uses compatible wlroots version
- [ ] **Scene node extensions** - scroll's custom nodes don't conflict with FX types
- [ ] **Renderer interface** - scroll's renderer implements full wlr_renderer interface
- [ ] **Damage tracking** - scroll's damage calculation compatible with blur expansion
- [ ] **Buffer lifecycle** - scroll's buffer management compatible with FX addon system
- [ ] **EGL context management** - scroll's GL usage compatible with FX context switching

---

## Key Takeaways

1. **SceneFX maintains wlroots API compatibility via struct inheritance** - Safe casting
2. **New functions are additions, not replacements** - Opt-in FX features
3. **Renderer creation is the main breaking change** - Must use `fx_renderer_create()`
4. **Enum extensions mostly compatible** - New node types skipped by old code
5. **Two-tier API (standard + FX)** - Standard code works, FX code gets more features
6. **No API stability guarantee** - Must vendor or fork SceneFX
7. **scroll's custom scene may conflict** - External daemon avoids conflicts

**Recommendation for scroll:**
- If scroll's scene changes are minor: **Full SceneFX integration**
- If scroll's scene changes are major: **External daemon approach**
- If unsure: **Start with external daemon, migrate to integrated later**
