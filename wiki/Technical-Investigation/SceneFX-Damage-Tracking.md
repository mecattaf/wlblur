# SceneFX Damage Tracking with Blur

**Investigation Date:** November 12, 2025
**Critical Topic:** Understanding damage region expansion for blur artifact prevention

---

## The Damage Tracking Problem with Blur

### Why Standard Damage Tracking Breaks with Blur

**Standard Wayland damage tracking:**
```
Window moves from (100,100) to (110,100)
→ Damage region: [100,100,10,h] (only the exposed edge)
→ Compositor re-renders only damaged region
```

**With blur enabled:**
```
Window moves from (100,100) to (110,100)
→ Blur kernel radius: 80 pixels
→ Problem: Blur at (110,100) needs pixels from (30,20) to (190,180)
→ If we only re-render [100,100,10,h], blur will sample stale pixels!
→ Result: Visual artifacts at blur edges
```

**Solution:** Expand damage regions by blur kernel size in all directions.

---

## Blur Size Calculation

### The Core Formula

**File:** `types/fx/blur_data.c` (lines 25-27)

```c
int blur_data_calc_size(struct blur_data *blur_data) {
    return pow(2, blur_data->num_passes + 1) * blur_data->radius;
}
```

**Formula:** `blur_size = 2^(num_passes + 1) × radius`

### Why This Formula?

Dual Kawase blur **doubles sampling area each pass:**

**Pass 0 (downsample):**
- Samples from: `uv ± halfpixel × radius`
- Effective kernel: `2 × radius`

**Pass 1 (downsample):**
- Input is half resolution
- Samples from: `uv ± halfpixel × radius` (at half res)
- Effective kernel in original coords: `2 × 2 × radius = 4 × radius`

**Pass 2 (downsample):**
- Input is quarter resolution
- Effective kernel: `2 × 4 × radius = 8 × radius`

**Pass N:**
- Effective kernel: `2^(N+1) × radius`

**For 3 passes:** `2^(3+1) × radius = 16 × radius`

**Example with defaults (radius=5, passes=3):**
```
blur_size = 2^4 × 5 = 16 × 5 = 80 pixels
```

This means a pixel at position (x,y) in the blurred result depends on pixels from:
```
(x - 80, y - 80) to (x + 80, y + 80)
```

---

## Three Levels of Damage Expansion

SceneFX applies damage expansion at three levels in the rendering pipeline:

### Level 1: Node Visibility Expansion

**File:** `types/scene/wlr_scene.c` (lines 680-684)

**When:** During scene graph traversal, before rendering

```c
static void update_node_visible(struct wlr_scene_node *node,
    struct wlr_scene_output *scene_output,
    struct scene_update_data *data) {

    // ... compute node's visible region ...

    if (node->type == WLR_SCENE_NODE_BLUR) {
        // Expand blur node's visibility to include kernel margin
        wlr_region_expand(&node->visible, &node->visible,
            blur_data_calc_size(data->blur_data));
    }
}
```

**Purpose:** Ensure blur nodes are considered "visible" even when partially off-screen, so they render enough context for edge pixels.

---

### Level 2: Render Pass Damage Expansion

**File:** `render/fx_renderer/fx_pass.c` (line 889)

**When:** At the start of blur rendering

```c
static void get_main_buffer_blur(struct fx_gles_render_pass *pass,
    const struct fx_render_blur_pass_options *fx_options,
    pixman_region32_t *damage, int blur_width, int blur_height) {

    // Apply strength scaling to blur parameters
    struct blur_data blur_data = blur_data_apply_strength(
        fx_options->blur_data, fx_options->blur_strength);

    // Expand damage region by blur kernel size
    wlr_region_expand(damage, damage, blur_data_calc_size(&blur_data));

    // ... proceed with blur passes ...
}
```

**Purpose:** Expand the damage region that will be processed by blur shaders, ensuring all pixels needed for blur computation are available.

**Example:**
```c
// Before expansion
damage = {x: 100, y: 100, width: 200, height: 200}

// blur_data_calc_size() returns 80

// After expansion
damage = {x: 20, y: 20, width: 360, height: 360}
```

---

### Level 3: Artifact Prevention with Saved Pixels

**File:** `types/scene/wlr_scene.c` (lines 2963-3010, 3079-3084)

**When:** During scene output rendering

**The Problem:**

Even with expanded damage, blur creates **edge artifacts** at the boundary between blurred and non-blurred regions:

```
┌─────────────────────────────────┐
│  Non-damaged region (stale)     │
│                                  │
│    ┌─────────────────────┐      │
│    │ Damaged + expanded  │      │
│    │ (re-rendered)        │      │
│    └─────────────────────┘      │
│         ↑                        │
│    Artifact zone: blur samples   │
│    mix fresh + stale pixels      │
└─────────────────────────────────┘
```

**The Solution:**

1. **Save padding pixels** before blur
2. **Apply blur** (may corrupt padding)
3. **Restore padding pixels** after blur

**Implementation:**

```c
static bool scene_output_build_state(struct wlr_scene_output *scene_output,
    struct wlr_output_state *state,
    const struct wlr_scene_output_state_options *options) {

    // ... render scene into framebuffer ...

    if (render_data.has_blur) {
        int blur_size = blur_data_calc_size(blur_data);

        // Calculate extended damage (original + blur margin)
        pixman_region32_t extended_damage;
        pixman_region32_init(&extended_damage);
        pixman_region32_copy(&extended_damage, damage);
        wlr_region_expand(&extended_damage, &extended_damage, blur_size);

        // Calculate padding region (extended - original)
        pixman_region32_subtract(&effect_fbos->blur_padding_region,
            &extended_damage, damage);

        // SAVE PADDING PIXELS before blur
        fx_renderer_read_to_buffer(render_pass,
            &effect_fbos->blur_padding_region,
            effect_fbos->blur_saved_pixels_buffer,
            render_pass->buffer);

        // ... apply blur ...

        // RESTORE PADDING PIXELS after blur
        fx_renderer_read_to_buffer(render_pass,
            &effect_fbos->blur_padding_region,
            render_pass->buffer,
            effect_fbos->blur_saved_pixels_buffer);
    }
}
```

**Visual Explanation:**

```
Step 1: Identify padding region
┌──────────────────────────────────┐
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│ ← Padding (extended - original)
│░░┌────────────────────────────┐░░│
│░░│  Original damage region    │░░│
│░░│  (window moved here)       │░░│
│░░└────────────────────────────┘░░│
│░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░│ ← Padding
└──────────────────────────────────┘

Step 2: Save padding pixels
memcpy(blur_saved_pixels_buffer, framebuffer, padding_region)

Step 3: Apply blur to extended region
blur(framebuffer, extended_damage)
→ Padding pixels are now corrupted with blur bleeding

Step 4: Restore padding pixels
memcpy(framebuffer, blur_saved_pixels_buffer, padding_region)
→ Clean boundary between blurred and non-blurred regions
```

---

## Damage Region Structures

### pixman_region32_t

SceneFX uses `pixman_region32_t` from the Pixman library for damage tracking:

```c
typedef struct pixman_region32 {
    pixman_box32_t extents;     // Bounding box of all rectangles
    pixman_region32_data_t *data;  // Array of rectangles
} pixman_region32_t;

typedef struct pixman_box32 {
    int32_t x1, y1;  // Top-left (inclusive)
    int32_t x2, y2;  // Bottom-right (exclusive)
} pixman_box32_t;
```

**Key Operations:**

```c
// Initialize empty region
pixman_region32_init(&region);

// Initialize with rectangle
pixman_region32_init_rect(&region, x, y, width, height);

// Copy region
pixman_region32_copy(&dest, &src);

// Union (A ∪ B)
pixman_region32_union(&result, &region_a, &region_b);

// Intersection (A ∩ B)
pixman_region32_intersect(&result, &region_a, &region_b);

// Subtraction (A - B)
pixman_region32_subtract(&result, &region_a, &region_b);

// Cleanup
pixman_region32_fini(&region);
```

### wlroots Damage Ring

**File:** `include/wlr/types/wlr_damage_ring.h`

```c
struct wlr_damage_ring {
    int32_t width, height;
    pixman_region32_t current;  // Damage since last frame
    pixman_region32_t previous[WLR_DAMAGE_RING_PREVIOUS_LEN];  // Ring buffer
};

// Accumulate damage
void wlr_damage_ring_add(&ring, &region);

// Get damage for rendering
void wlr_damage_ring_get_buffer_damage(&ring, age, &output_damage);

// Rotate after render
void wlr_damage_ring_rotate(&ring);
```

**Per-output damage ring:**
```c
struct wlr_scene_output {
    struct wlr_output *output;
    struct wlr_damage_ring damage_ring;  // Tracks accumulated damage
    // ...
};
```

---

## Damage Expansion Helpers

### wlr_region_expand()

**File:** `include/wlr/util/region.h`

```c
/**
 * Expand a region by distance in all directions.
 * Equivalent to Minkowski sum with a square of side 2*distance.
 */
void wlr_region_expand(pixman_region32_t *dst,
    const pixman_region32_t *src, int distance);
```

**Implementation (conceptual):**
```c
void wlr_region_expand(pixman_region32_t *dst,
    const pixman_region32_t *src, int distance) {

    int n_rects;
    const pixman_box32_t *src_rects = pixman_region32_rectangles(src, &n_rects);

    pixman_region32_t expanded;
    pixman_region32_init(&expanded);

    for (int i = 0; i < n_rects; i++) {
        pixman_box32_t expanded_box = {
            .x1 = src_rects[i].x1 - distance,
            .y1 = src_rects[i].y1 - distance,
            .x2 = src_rects[i].x2 + distance,
            .y2 = src_rects[i].y2 + distance,
        };

        pixman_region32_t box_region;
        pixman_region32_init_rects(&box_region, &expanded_box, 1);
        pixman_region32_union(&expanded, &expanded, &box_region);
        pixman_region32_fini(&box_region);
    }

    pixman_region32_copy(dst, &expanded);
    pixman_region32_fini(&expanded);
}
```

**Example:**
```c
// Original damage
pixman_region32_t damage;
pixman_region32_init_rect(&damage, 100, 100, 50, 50);
// → box: [100,100] to [150,150]

// Expand by 80 pixels
wlr_region_expand(&damage, &damage, 80);
// → box: [20,20] to [230,230]
```

---

### wlr_region_scale()

**File:** `include/wlr/util/region.h`

```c
/**
 * Scale a region by a floating-point factor.
 * Used for scaling damage between different mipmap levels.
 */
void wlr_region_scale(pixman_region32_t *dst,
    const pixman_region32_t *src, float scale);
```

**Usage in blur passes:**

```c
// During downsample pass i
// Scale damage to match mipmap level
float scale = 1.0f / (1 << (i + 1));
wlr_region_scale(&scaled_damage, &damage, scale);

// Example: Pass 1
// scale = 1.0 / (1 << 2) = 1.0 / 4 = 0.25
// damage [100,100,200,200] → scaled_damage [25,25,50,50]
```

---

## Optimization: Whole-Output Blur Detection

**File:** `types/scene/wlr_scene.c` (lines 2972-2988)

**Problem:** If the entire output needs blur, expanding damage wastes GPU time (whole screen is already damaged).

**Optimization:**
```c
if (render_data.has_blur) {
    pixman_box32_t *damage_extents = pixman_region32_extents(damage);
    int damage_width = damage_extents->x2 - damage_extents->x1;
    int damage_height = damage_extents->y2 - damage_extents->y1;

    // Check if damage already covers whole output
    if (damage_width <= output_width && damage_height <= output_height) {
        // Damage is partial, apply expansion and artifact prevention
        apply_blur_with_artifact_prevention();
    } else {
        // Damage is whole output, skip artifact prevention
        // (no clean boundary to preserve)
        apply_blur_simple();
    }
}
```

**Rationale:** When the whole output is damaged (e.g., first frame, resolution change), there's no "non-blurred" region to protect, so skip the pixel save/restore.

---

## Damage Tracking for Optimized Blur

### The Dirty Flag Pattern

**File:** `include/scenefx/types/wlr_scene.h` (lines 187-192)

```c
struct wlr_scene_optimized_blur {
    struct wlr_scene_node node;
    int width, height;
    bool dirty;  // Invalidation flag
};
```

**Marking dirty:**
```c
void wlr_scene_optimized_blur_mark_dirty(struct wlr_scene_optimized_blur *blur_node) {
    if (blur_node->dirty) {
        return;  // Already dirty
    }

    blur_node->dirty = true;
    scene_node_update(&blur_node->node, NULL);  // Trigger re-render
}
```

**When to mark dirty:**
- Wallpaper changes
- Output resolution changes
- Global blur parameters change (`wlr_scene_set_blur_radius()`, etc.)
- Output scale changes

**Rendering logic:**
```c
case WLR_SCENE_NODE_OPTIMIZED_BLUR:
    if (blur_node->dirty) {
        // Re-render cached blur
        render_optimized_blur(blur_node);
        blur_node->dirty = false;
    }

    // Use cached texture (no re-rendering)
    composite_optimized_blur_texture();
    break;
```

**Damage implications:**
- **Dirty blur:** Entire blur region is damaged
- **Clean blur:** Zero damage (texture is cached)

---

## Edge Cases and Gotchas

### Edge Case 1: Blur Extends Beyond Output Bounds

```
Output: 1920×1080
Window at: (1800, 1000)
Blur radius: 80 pixels
→ Blur needs pixels at (1880, 1080) which don't exist
```

**Solution:** Clamp damage regions to output bounds

```c
pixman_region32_t output_region;
pixman_region32_init_rect(&output_region, 0, 0, output_width, output_height);

// After expansion
pixman_region32_intersect(&expanded_damage, &expanded_damage, &output_region);
```

**File:** `types/scene/wlr_scene.c` (line 2977)

---

### Edge Case 2: Overlapping Blur Regions

```
Window A at (100, 100) with blur
Window B at (150, 150) with blur
→ Blur regions overlap
→ Each needs the other's blurred result
```

**Solution:** SceneFX renders blur bottom-to-top

```c
// Scene graph order (bottom to top):
1. Background
2. Window A
3. Blur A (blurs background + Window A)
4. Window B
5. Blur B (blurs background + Window A + Blur A + Window B)
```

**Implication:** Damage tracking must consider blur ordering. If Window A moves, both Blur A and Blur B must be re-rendered.

---

### Edge Case 3: Nested Blur (Blur Inside Blur)

```
Parent blur region contains child blur region
→ Child blur should blur parent's result
```

**SceneFX behavior:** Each blur is rendered independently during scene traversal. Nested blurs work correctly because scene graph is traversed bottom-to-top.

**Damage implication:** Parent blur damage must include child blur margin.

---

## Performance Implications

### Damage Expansion Cost

**Memory:**
- Expanded regions contain more rectangles
- Pixman coalesces overlapping rectangles (optimized)
- Worst case: O(n²) rectangle count if no coalescence

**GPU:**
- Larger damage → more pixels to render
- blur_size=80 → ~6.4x area (80px margin on all sides)
- 1920×1080 full screen blur: ~2.1M pixels
- 200×200 window blur: ~360k pixels → with expansion: ~2.3M pixels (!)

**Optimization:** Use `should_only_blur_bottom_layer` to avoid blurring windows above:

```c
wlr_scene_blur_set_should_only_blur_bottom_layer(blur, true);
```

This reduces blur kernel input size, speeding up rendering.

---

## Damage Tracking API Summary

### For Compositor Authors

**Enable blur:**
```c
// Set global blur parameters
wlr_scene_set_blur_data(scene, num_passes, radius, noise, brightness, contrast, saturation);

// Create blur node
struct wlr_scene_blur *blur = wlr_scene_blur_create(parent, width, height);

// Damage tracking is automatic!
```

**Manual damage control:**
```c
// Mark specific region as damaged
pixman_region32_t region;
pixman_region32_init_rect(&region, x, y, width, height);
wlr_damage_ring_add(&scene_output->damage_ring, &region);
pixman_region32_fini(&region);
```

**Optimized blur (for static content):**
```c
// Create optimized blur
struct wlr_scene_optimized_blur *opt_blur =
    wlr_scene_optimized_blur_create(parent, width, height);

// When wallpaper changes
wlr_scene_optimized_blur_mark_dirty(opt_blur);
```

---

## Key Takeaways

1. **Blur size formula is critical:** `2^(passes+1) × radius` determines margin
2. **Three-level damage expansion:** Node visibility, render pass, artifact prevention
3. **Artifact prevention requires pixel save/restore:** Extended damage region strategy
4. **Whole-output detection skips artifact prevention:** Performance optimization
5. **Optimized blur uses dirty flag:** Avoid re-rendering static content
6. **Damage expansion can be expensive:** ~6x area increase with default blur

**For External Daemon:**
- Daemon must compute blur_size and expand damage regions before sending to compositor
- Compositor must send expanded damage, not original client damage
- Artifact prevention can be compositor-side (daemon doesn't need to handle)
- Cached blur requires daemon to track dirty state per surface
