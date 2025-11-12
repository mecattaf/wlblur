# Wayfire Blur Plugin Architecture

## Overview

Wayfire's blur implementation demonstrates a **modular plugin architecture** that cleanly separates blur effects from the compositor's core rendering logic. This makes it the ideal reference implementation for extracting blur functionality into an external daemon process.

**Repository**: [https://github.com/WayfireWM/wayfire](https://github.com/WayfireWM/wayfire)
**Primary Language**: C++
**Graphics API**: OpenGL ES 3.0
**Plugin Location**: `plugins/blur/`

---

## Plugin System Architecture

### High-Level Design

```
┌─────────────────────────────────────────────────────────┐
│                    Wayfire Core                         │
│  ┌───────────────────────────────────────────────────┐  │
│  │           Scene Graph / Rendering Pipeline        │  │
│  │  • Manages compositor state                       │  │
│  │  • Provides OpenGL ES 3.0 context                 │  │
│  │  • Handles damage tracking & frame scheduling     │  │
│  └───────────────────────────────────────────────────┘  │
│                          ▲                              │
│                          │                              │
│                          │ Plugin API                   │
│                          │                              │
│  ┌───────────────────────────────────────────────────┐  │
│  │              Blur Plugin (Dynamic Module)         │  │
│  │  ┌─────────────────────────────────────────────┐  │  │
│  │  │  wayfire_blur : plugin_interface_t          │  │  │
│  │  │  • Lifecycle: init() / fini()               │  │  │
│  │  │  • Configuration loading                    │  │  │
│  │  │  • View transformer management              │  │  │
│  │  └─────────────────────────────────────────────┘  │  │
│  │              ▼                                      │  │
│  │  ┌─────────────────────────────────────────────┐  │  │
│  │  │  blur_node_t : transformer_base_node_t      │  │  │
│  │  │  • Scene graph integration                  │  │  │
│  │  │  • Per-view blur transformations            │  │  │
│  │  └─────────────────────────────────────────────┘  │  │
│  │              ▼                                      │  │
│  │  ┌─────────────────────────────────────────────┐  │  │
│  │  │  wf_blur_base (Algorithm Provider)          │  │  │
│  │  │  • Manages FBOs and shaders                 │  │  │
│  │  │  • Algorithm selection (Kawase/Gaussian/etc)│  │  │
│  │  │  • Blur preparation & rendering             │  │  │
│  │  └─────────────────────────────────────────────┘  │  │
│  └───────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────┘
```

---

## Plugin Lifecycle

### 1. Plugin Registration

Wayfire plugins are dynamically loaded shared libraries that register using the `DECLARE_WAYFIRE_PLUGIN` macro:

**File**: `plugins/blur/blur.cpp:399`
```cpp
DECLARE_WAYFIRE_PLUGIN(wayfire_blur);
```

This macro generates the boilerplate needed for Wayfire's plugin loader to:
- Discover the plugin at runtime
- Instantiate the plugin class
- Call lifecycle methods at appropriate times

### 2. Initialization (`init()`)

**File**: `plugins/blur/blur.cpp:335-387`

The `wayfire_blur::init()` method performs:

#### a. Renderer Validation
```cpp
if (!wf::get_core().is_gles2()) {
    LOGE("blur: requires GLES2 support, but current renderer is ", render_type);
    return;
}
```
The plugin checks that the compositor is running with OpenGL ES support. This is **critical** - the blur plugin cannot work with Vulkan or Pixman backends.

#### b. Algorithm Initialization
```cpp
blur_method_changed = [=] () {
    blur_algorithm = create_blur_from_name(method_opt);
    wf::scene::damage_node(wf::get_core().scene(),
                          wf::get_core().scene()->get_bounding_box());
};
blur_method_changed();
method_opt.set_callback(blur_method_changed);
```

The plugin:
1. Reads the `blur/method` configuration option
2. Creates the appropriate algorithm implementation (Kawase, Gaussian, Box, or Bokeh)
3. Registers a callback to recreate the algorithm if the user changes the config
4. Damages the entire scene to trigger a re-render with the new algorithm

#### c. Render Pass Hook Registration
```cpp
wf::get_core().connect(&on_render_pass_begin);
```

The plugin registers a **damage expansion hook** that runs before each render pass:

**File**: `plugins/blur/blur.cpp:272-284`
```cpp
on_render_pass_begin = [=] (wf::render_pass_begin_signal *ev) {
    const int padding =
        calculate_damage_padding(ev->pass.get_target(),
                                provider()->calculate_blur_radius());
    ev->damage.expand_edges(padding);
    ev->damage &= ev->pass.get_target().geometry;
};
```

**Why this is needed**: When a region is damaged (e.g., a window redraws), blurring that region requires sampling pixels **beyond** the damaged area. Without expansion, blur would create artifacts at damage boundaries. The padding ensures we re-render enough surrounding context for smooth blur.

#### d. View Management Setup
```cpp
wf::get_core().connect(&on_view_mapped);

on_view_mapped = [=] (wf::view_mapped_signal *ev) {
    if (blur_by_default.matches(ev->view)) {
        add_transformer(ev->view);
    }
};
```

When a new window (view) appears:
1. Check if it matches the `blur_by_default` criteria (configurable via `blur.xml`)
2. If yes, attach a `blur_node_t` transformer to the view's scene graph node

#### e. Existing Views Processing
```cpp
for (auto& view : wf::get_core().get_all_views()) {
    if (blur_by_default.matches(view)) {
        add_transformer(view);
    }
}
```

Apply blur to all currently-open windows that match the criteria.

#### f. Keybinding Registration
```cpp
button_toggle = [=] (auto) {
    auto view = wf::get_core().get_cursor_focus_view();
    if (view->get_transformed_node()->get_transformer<wf::scene::blur_node_t>()) {
        pop_transformer(view);
    } else {
        add_transformer(view);
    }
    return true;
};
wf::get_core().bindings->add_button(toggle_button, &button_toggle);
```

The user can toggle blur on/off for the focused window using a configurable button/key binding.

### 3. Finalization (`fini()`)

**File**: `plugins/blur/blur.cpp:389-396`

```cpp
void fini() override {
    remove_transformers();  // Remove blur from all views
    wf::get_core().bindings->rem_binding(&button_toggle);
    blur_algorithm = nullptr;  // Destructor frees GL resources
}
```

Clean shutdown:
1. Remove all blur transformers from views
2. Unregister keybindings
3. Destroy the blur algorithm (which triggers OpenGL resource cleanup)

---

## Scene Graph Integration

Wayfire uses a **scene graph** architecture where visual effects are **transformer nodes** attached to views.

### Adding Blur to a View

**File**: `plugins/blur/blur.cpp:303-318`

```cpp
void add_transformer(wayfire_view view) {
    auto tmanager = view->get_transformed_node();
    if (tmanager->get_transformer<wf::scene::blur_node_t>()) {
        return;  // Already has blur
    }

    auto provider = [=] () { return blur_algorithm.get(); };
    auto node = std::make_shared<wf::scene::blur_node_t>(provider);
    tmanager->add_transformer(node, wf::TRANSFORMER_BLUR);
}
```

This creates a `blur_node_t` that:
- Receives a **provider function** that returns the current blur algorithm
- Gets inserted into the view's transformation stack at the `TRANSFORMER_BLUR` priority
- Will be invoked during rendering to apply blur effects

### Blur Node Structure

**File**: `plugins/blur/blur.cpp:46-91`

```cpp
class blur_node_t : public transformer_base_node_t {
    blur_algorithm_provider provider;
    std::list<saved_pixels_t> saved_pixels;

    // Generates render instances for this node
    void gen_render_instances(std::vector<render_instance_uptr>& instances,
                              damage_callback push_damage,
                              wf::output_t *shown_on) override;
};
```

The `blur_node_t`:
- Holds a reference to the blur algorithm provider
- Maintains a pool of `saved_pixels_t` buffers for damage artifact mitigation
- Creates `blur_render_instance_t` objects when rendering

---

## Rendering Pipeline

### Render Instance Creation

**File**: `plugins/blur/blur.cpp:253-262`

When the scene graph is traversed for rendering, each `blur_node_t` creates a `blur_render_instance_t`:

```cpp
void blur_node_t::gen_render_instances(...) {
    auto uptr = std::make_unique<blur_render_instance_t>(this, push_damage, shown_on);
    if (uptr->has_instances()) {
        instances.push_back(std::move(uptr));
    }
}
```

### Damage Padding Strategy

**File**: `plugins/blur/blur.cpp:131-176`

The render instance's `schedule_instructions()` method implements Wayfire's sophisticated **damage padding** mechanism:

```cpp
void schedule_instructions(std::vector<render_instruction_t>& instructions,
                          const wf::render_target_t& target,
                          wf::region_t& damage) override {
    const int padding = calculate_damage_padding(target,
                                                 self->provider()->calculate_blur_radius());
    auto bbox = self->get_bounding_box();
    auto padded_region = damage & bbox;

    if (is_fully_opaque(padded_region & target.geometry)) {
        // No translucent regions to blur - skip
        for (auto& ch : this->children) {
            ch->schedule_instructions(instructions, target, damage);
        }
        return;
    }

    // Expand damage for blur sampling
    padded_region.expand_edges(padding);
    padded_region &= bbox;
    padded_region &= target.geometry;

    // Save pixels in the padding area (to restore later)
    this->saved_pixels = self->acquire_saved_pixel_buffer();
    saved_pixels->region =
        target.framebuffer_region_from_geometry_region(padded_region) ^
        target.framebuffer_region_from_geometry_region(damage);

    // Tell nodes below to re-render the padded areas
    damage |= padded_region;

    // Copy padded pixels to saved buffer
    saved_pixels->pixels.allocate(target.get_size());
    // ... glBlitFramebuffer calls ...
}
```

**The algorithm**:
1. Calculate required padding based on blur radius
2. Check if there's any translucent area to blur (optimization)
3. Expand the damage region by the padding amount
4. **Save** the pixels in the padding area (before blurring)
5. Request child nodes to re-render the expanded area
6. Store the saved pixels for later restoration

### Blur Rendering

**File**: `plugins/blur/blur.cpp:205-244`

The `render()` method applies the actual blur effect:

```cpp
void render(const wf::scene::render_instruction_t& data) override {
    auto bounding_box = self->get_bounding_box();
    data.pass->custom_gles_subpass([&] {
        auto tex = wf::gles_texture_t{get_texture(data.target.scale)};
        if (!data.damage.empty()) {
            auto translucent_damage = calculate_translucent_damage(data.target, data.damage);
            // Prepare blurred background
            self->provider()->prepare_blur(data.target, translucent_damage);
            // Composite blurred background with view texture
            self->provider()->render(tex, bounding_box, data.damage,
                                    data.target, data.target);
        }

        // Restore saved pixels to eliminate padding artifacts
        GL_CALL(glDisable(GL_SCISSOR_TEST));
        GLuint saved_fb = wf::gles::ensure_render_buffer_fb_id(saved_pixels->pixels.get_renderbuffer());
        wf::gles::bind_render_buffer(data.target);
        GL_CALL(glBindFramebuffer(GL_READ_FRAMEBUFFER, saved_fb));

        for (const auto& box : saved_pixels->region) {
            GL_CALL(glBlitFramebuffer(/* Copy saved pixels back */));
        }

        saved_pixels->region.clear();
        self->release_saved_pixel_buffer(saved_pixels);
    });
}
```

**The process**:
1. Get the view's texture
2. Calculate which regions actually need blur (skip fully opaque areas)
3. Call `prepare_blur()` to generate the blurred background
4. Call `render()` to composite the blurred background with the view
5. **Restore the saved padding pixels** - this eliminates artifacts that appeared in the padding area during blur
6. Release the saved pixel buffer for reuse

---

## Blur Algorithm Abstraction

### Base Class Interface

**File**: `plugins/blur/blur.hpp:91-155`

```cpp
class wf_blur_base {
protected:
    wf::auxilliary_buffer_t fb[2];        // Ping-pong buffers for multi-pass
    OpenGL::program_t program[2];         // Shader programs
    OpenGL::program_t blend_program;      // Final compositing shader

    std::string algorithm_name;
    wf::option_wrapper_t<double> saturation_opt, offset_opt;
    wf::option_wrapper_t<int> degrade_opt, iterations_opt;

    // Pure virtual - each algorithm implements its own blur logic
    virtual int blur_fb0(const wf::region_t& blur_region,
                        int width, int height) = 0;

public:
    virtual int calculate_blur_radius();

    void prepare_blur(const wf::render_target_t& target_fb,
                     const wf::region_t& damage);

    void render(wf::gles_texture_t src_tex, wlr_box src_box,
               const wf::region_t& damage,
               const wf::render_target_t& background_source_fb,
               const wf::render_target_t& target_fb);
};
```

### Factory Pattern

**File**: `plugins/blur/blur-base.cpp:304-328`

```cpp
std::unique_ptr<wf_blur_base> create_blur_from_name(std::string algorithm_name) {
    if (algorithm_name == "box")     return create_box_blur();
    if (algorithm_name == "bokeh")   return create_bokeh_blur();
    if (algorithm_name == "kawase")  return create_kawase_blur();
    if (algorithm_name == "gaussian") return create_gaussian_blur();

    LOGE("Unrecognized blur algorithm %s. Using default kawase blur.",
         algorithm_name.c_str());
    return create_kawase_blur();
}
```

Each algorithm is implemented as a subclass of `wf_blur_base`:
- `wf_kawase_blur` - Dual Kawase (downsample/upsample)
- `wf_gaussian_blur` - Separable Gaussian (horizontal/vertical passes)
- `wf_box_blur` - Simple box filter (horizontal/vertical passes)
- `wf_bokeh_blur` - Depth-of-field style blur (golden angle sampling)

---

## OpenGL Resource Management

### Context Safety

All OpenGL operations are wrapped in `wf::gles::run_in_context_if_gles()` to ensure the compositor's GL context is current:

**File**: `plugins/blur/blur-base.cpp:77-80`
```cpp
wf::gles::run_in_context_if_gles([&] {
    blend_program.compile(blur_blend_vertex_shader, blur_blend_fragment_shader);
});
```

### Resource Cleanup

**File**: `plugins/blur/blur-base.cpp:83-91`
```cpp
wf_blur_base::~wf_blur_base() {
    wf::gles::run_in_context_if_gles([&] {
        program[0].free_resources();
        program[1].free_resources();
        blend_program.free_resources();
    });
}
```

The base destructor ensures all shader programs are properly freed when the algorithm changes or the plugin unloads.

---

## Configuration System

### Configuration File

**File**: `metadata/blur.xml`

Wayfire uses XML metadata to define plugin configuration options:

```xml
<option name="method" type="string">
    <default>kawase</default>
    <desc>
        <value>box</value>
        <value>gaussian</value>
        <value>kawase</value>
        <value>bokeh</value>
    </desc>
</option>

<option name="saturation" type="double">
    <default>1.0</default>
    <min>0.0</min>
    <max>3.0</max>
</option>

<!-- Per-algorithm parameters -->
<option name="kawase_offset" type="double">
    <default>1.7</default>
    <min>0</min>
    <max>25</max>
</option>
```

### Configuration Loading

**File**: `plugins/blur/blur-base.cpp:59-76`

```cpp
wf_blur_base::wf_blur_base(std::string name) {
    this->algorithm_name = name;

    this->saturation_opt.load_option("blur/saturation");
    this->offset_opt.load_option("blur/" + algorithm_name + "_offset");
    this->degrade_opt.load_option("blur/" + algorithm_name + "_degrade");
    this->iterations_opt.load_option("blur/" + algorithm_name + "_iterations");

    // Damage scene when options change
    this->options_changed = [=] () {
        wf::scene::damage_node(wf::get_core().scene(),
                              wf::get_core().scene()->get_bounding_box());
    };
    this->saturation_opt.set_callback(options_changed);
    this->offset_opt.set_callback(options_changed);
    this->degrade_opt.set_callback(options_changed);
    this->iterations_opt.set_callback(options_changed);
}
```

When any blur parameter changes:
1. The callback is invoked
2. The entire scene is marked as damaged
3. On the next frame, blur re-renders with new parameters

---

## Key Architectural Insights

### 1. Clean Plugin Boundary

The blur plugin:
- ✅ Has no direct dependencies on compositor internals
- ✅ Receives GL context through a well-defined API
- ✅ Manages its own FBOs, shaders, and state
- ✅ Uses compositor-provided damage tracking
- ✅ Integrates via scene graph transformer pattern

This makes it **straightforward to extract** into an external process.

### 2. Algorithm Independence

The `wf_blur_base` abstraction means:
- Multiple blur algorithms can coexist
- Users can switch algorithms at runtime
- Each algorithm has independent parameters
- Adding new algorithms doesn't require compositor changes

### 3. Damage-Aware Design

The sophisticated damage padding mechanism:
- Avoids full-screen re-renders
- Minimizes GPU work by only processing damaged regions
- Prevents blur artifacts at damage boundaries
- Reuses cached pixels where possible

### 4. Per-View Granularity

Blur is applied at the **view (window) level**, not globally:
- Each window can have blur independently
- Blur can be toggled per-window
- Blur parameters are shared (not per-window), but the architecture could support per-window tuning

---

## Next Steps

See the companion documents:
- **02-algorithm-analysis.md** - Detailed breakdown of each blur algorithm
- **03-damage-tracking.md** - Deep dive into damage padding and caching
- **04-ipc-feasibility.md** - How to extract this into an external daemon

---

## File Reference

| Category | Files | Purpose |
|----------|-------|---------|
| **Plugin Entry** | `blur.cpp` | Plugin lifecycle, scene graph integration |
| **Base Class** | `blur.hpp`, `blur-base.cpp` | Algorithm abstraction, FBO management |
| **Algorithms** | `kawase.cpp`, `gaussian.cpp`, `box.cpp`, `bokeh.cpp` | Shader implementations |
| **Configuration** | `metadata/blur.xml` | User-facing settings |

All files located in the Wayfire repository at `plugins/blur/` and `metadata/`.
