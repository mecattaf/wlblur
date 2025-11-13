before wrapping up milestone 2 actually there is one more discussion oint i want to bring up:

```
Must address the pros and cons of having a dedicated blur provider configuration vs having it done in-config.

Since it's exposed to a socket after all, we could just start it with `exec wlblur`and like kanshi add some flags to it like `-config` for nonstandard locations

And - since we can - make the daemon hot-reloadable so that we can dynamically experiment with different configurations.

```


when moving between multiple compositors that have this, it could be interesting to make the blur configuration externalized entirely. 

give me a full write up about this that i can hand off to another llm to discuss this point in depth

---
Claude said:
# Configuration Architecture Discussion: External Config vs IPC-Only Configuration

## Context for Discussion

**Project**: wlblur - External blur daemon for Wayland compositors  
**Current State**: Milestone m-2 complete, IPC protocol implemented  
**Decision Needed**: Configuration architecture before compositor integration (m-3)

## The Question

Should wlblurd have:
1. **Dedicated configuration system** (external config file + daemon manages settings)
2. **IPC-only configuration** (compositor sends all parameters per request)
3. **Hybrid approach** (defaults in daemon config, overrides via IPC)

This impacts multi-compositor workflows and user experience significantly.

---

## Current IPC Protocol Design

From `docs/api/ipc-protocol.md`:

```c
struct wlblur_request {
    uint32_t protocol_version;
    uint32_t op;  // CREATE_NODE, RENDER_BLUR, DESTROY_NODE
    uint32_t node_id;
    
    // Texture dimensions
    uint32_t width;
    uint32_t height;
    
    // DMA-BUF format info
    uint32_t format;
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;
    
    // Blur parameters (sent per-request)
    struct wlblur_blur_params params;
};

struct wlblur_blur_params {
    int num_passes;      // 1-8, default 3
    float radius;        // 1.0-20.0, default 5.0
    float brightness;    // 0.0-2.0, default 1.0
    float contrast;      // 0.0-2.0, default 1.0
    float saturation;    // 0.0-2.0, default 1.0
    float noise;         // 0.0-1.0, default 0.02
    float vibrancy;      // 0.0-2.0, default 0.0
};
```

**Current design**: Compositor sends full parameters with every blur request.

---

## Approach 1: Dedicated Configuration System

### Architecture

```
~/.config/wlblur/config.toml  (user configuration)
↓
wlblurd reads on startup
↓
Daemon maintains default parameters
↓
Compositor can override per-request (optional)
```

### Configuration File Example

```toml
# ~/.config/wlblur/config.toml

[daemon]
socket_path = "/run/user/1000/wlblur.sock"
log_level = "info"
max_nodes_per_client = 100

[defaults]
# Default blur parameters for all requests
num_passes = 3
radius = 5.0
brightness = 1.0
contrast = 1.0
saturation = 1.1
noise = 0.02
vibrancy = 0.0

[presets.window]
# Preset for regular windows
num_passes = 3
radius = 8.0
saturation = 1.15

[presets.panel]
# Preset for panels/bars
num_passes = 2
radius = 4.0
brightness = 1.1

[presets.hud]
# Preset for HUD/overlay elements
num_passes = 4
radius = 12.0
vibrancy = 0.2

[performance]
# Performance tuning
enable_caching = true
cache_size_mb = 64
```

### Daemon Startup

```bash
# Standard location
wlblurd

# Custom config
wlblurd --config ~/.config/wlblur/custom.toml

# Hot reload on SIGUSR1
killall -USR1 wlblurd
```

### Modified IPC Protocol

```c
struct wlblur_request {
    // ... existing fields ...
    
    // Configuration options
    bool use_defaults;           // Use daemon defaults
    const char* preset_name;     // "window", "panel", "hud", null
    struct wlblur_blur_params params;  // Override if provided
};
```

**Fallback hierarchy:**
1. Compositor-provided params (if present)
2. Named preset (if specified)
3. Daemon defaults (from config file)

### Implementation Requirements

**New files needed:**
- `wlblurd/src/config.c` - Config file parsing (TOML library)
- `wlblurd/include/config.h` - Config structures
- `wlblurd/src/presets.c` - Preset management
- `wlblurd/src/reload.c` - Hot reload via signals

**Dependencies:**
- TOML parsing library (tomlc99 or similar)
- Signal handling for hot reload

**Code estimate:** +400-600 lines

---

## Approach 2: IPC-Only Configuration

### Architecture

```
Compositor config file (e.g., scroll.toml)
↓
Compositor reads blur parameters
↓
Compositor sends full parameters with every request
↓
Daemon is stateless (no configuration)
```

### Compositor Configuration Example

**ScrollWM config:**
```toml
[blur]
enabled = true
daemon_socket = "/run/user/1000/wlblur.sock"
num_passes = 3
radius = 5.0
saturation = 1.1
```

**niri config:**
```kdl
blur {
    enabled true
    daemon-socket "/run/user/1000/wlblur.sock"
    num-passes 3
    radius 5.0
    saturation 1.1
}
```

### Daemon Behavior

- **No config file** - daemon starts with hardcoded socket path
- **Stateless** - no memory of previous requests
- **Parameters required** - compositor must send complete params every time

### Implementation Requirements

**Existing code works as-is** - no changes needed to daemon.

**Compositor responsibility:**
- Parse blur config
- Maintain parameter state
- Send with every request

**Code estimate:** 0 lines (already implemented)

---

## Approach 3: Hybrid (Daemon Defaults + Compositor Overrides)

### Architecture

```
~/.config/wlblur/defaults.toml  (optional, system-wide defaults)
↓
Compositor config (compositor-specific overrides)
↓
Per-request overrides (runtime adjustments)
```

### Example Flow

**User's wlblur config:**
```toml
# ~/.config/wlblur/defaults.toml
[defaults]
num_passes = 3
radius = 5.0
saturation = 1.1
```

**ScrollWM config:**
```toml
[blur]
enabled = true
# Inherits wlblur defaults
# Override only what's different:
radius = 7.0  # Different from daemon default
```

**Runtime override:**
```c
// Compositor can override at runtime for specific windows
if (window_is_panel(window)) {
    params.radius = 4.0;  // Panel-specific override
}
```

### Implementation

**Daemon:**
- Reads optional config file
- Provides defaults if compositor doesn't specify
- Accepts full overrides from compositor

**IPC Protocol:**
```c
struct wlblur_request {
    // ... existing fields ...
    
    bool use_daemon_defaults;    // Use daemon's config
    struct wlblur_blur_params params;  // Overrides (null values = use default)
};
```

**Code estimate:** +200-300 lines (minimal config parsing)

---

## Multi-Compositor Workflow Analysis

### Scenario: User switches between compositors

**User setup:**
- Uses ScrollWM for tiling workflow
- Uses niri for specific tasks
- Uses Sway occasionally
- Wants consistent blur appearance

### With Approach 1 (Daemon Config)

```bash
# Single config file for all compositors
~/.config/wlblur/config.toml

[defaults]
radius = 5.0
saturation = 1.1
```

**User experience:**
- ✅ Configure once, works everywhere
- ✅ Switch compositors, blur looks identical
- ✅ Central management
- ⚠️ Can't have compositor-specific preferences easily

### With Approach 2 (IPC-Only)

```bash
# Each compositor has its own config
~/.config/scroll/config.toml
~/.config/niri/config.kdl
~/.config/sway/config
```

**User experience:**
- ⚠️ Must configure blur in each compositor
- ✅ Full flexibility per compositor
- ⚠️ Inconsistency if configs drift
- ⚠️ More maintenance

### With Approach 3 (Hybrid)

```bash
# System default
~/.config/wlblur/defaults.toml

# Compositor-specific overrides
~/.config/scroll/config.toml  (overrides only radius)
~/.config/niri/config.kdl     (accepts all defaults)
~/.config/sway/config         (overrides saturation)
```

**User experience:**
- ✅ Configure common settings once
- ✅ Customize per-compositor when needed
- ✅ Balanced maintenance
- ⚠️ Slightly more complex mental model

---

## Hot Reload Implications

### With Daemon Config (Approach 1/3)

**User workflow:**
```bash
# Edit config
vim ~/.config/wlblur/config.toml

# Reload daemon
killall -USR1 wlblurd
# or
wlblur-reload

# Changes apply immediately to all compositors
```

**Implementation:**
```c
// wlblurd/src/main.c
static void handle_sigusr1(int sig) {
    reload_config();
    recompute_all_nodes();
}

signal(SIGUSR1, handle_sigusr1);
```

**Benefits:**
- ✅ Live experimentation with parameters
- ✅ No compositor restart needed
- ✅ Changes apply to all running compositors

**Risks:**
- ⚠️ Config file parse errors could break daemon
- ⚠️ Need validation before applying changes

### With IPC-Only Config (Approach 2)

**User workflow:**
```bash
# Edit compositor config
vim ~/.config/scroll/config.toml

# Reload compositor
scrollctl reload
# or restart compositor
```

**Benefits:**
- ✅ No daemon-side complexity
- ✅ Compositor handles validation
- ✅ Already familiar to users

**Risks:**
- ⚠️ Compositor restart might be disruptive
- ⚠️ Can't experiment without restarting display server

---

## Precedent from Similar Tools

### kanshi (Output configuration daemon)

```bash
# Has dedicated config file
~/.config/kanshi/config

# Reloads on SIGHUP
kanshi &
vim ~/.config/kanshi/config
killall -HUP kanshi
```

**Lessons:**
- Dedicated config works well for cross-compositor tools
- Hot reload is expected functionality
- Users understand separate config files

### mako (Notification daemon)

```bash
# Has dedicated config file
~/.config/mako/config

# Reloads on SIGHUP
makoctl reload
```

**Lessons:**
- External config is standard for Wayland tools
- Reload command is user-friendly
- Integration with compositor configs is minimal

### swayidle (Idle management)

```bash
# Configured via command-line flags
swayidle -w \
    timeout 300 'swaylock' \
    timeout 600 'swaymsg "output * dpms off"'
```

**Lessons:**
- Simple tools use CLI flags
- More complex needs might benefit from config file
- Started by compositor init script

### gammastep (Color temperature)

```bash
# Has config file and CLI overrides
~/.config/gammastep/config.ini

# CLI overrides config
gammastep -l 40.7:-74.0 -t 5700:3600
```

**Lessons:**
- Hybrid approach works well
- Config file for defaults
- CLI for overrides/experimentation

---

## Technical Considerations

### Configuration File Format Options

**TOML (Recommended):**
```toml
[blur]
radius = 5.0
num_passes = 3
```
- ✅ Human-readable
- ✅ Typed values
- ✅ Good library support (tomlc99)
- ✅ Common in Wayland ecosystem

**JSON:**
```json
{
  "blur": {
    "radius": 5.0,
    "num_passes": 3
  }
}
```
- ✅ Simple parsing
- ⚠️ Less human-friendly
- ⚠️ No comments

**INI/key-value:**
```ini
blur.radius=5.0
blur.num_passes=3
```
- ✅ Simplest parsing
- ⚠️ Limited structure
- ⚠️ No typing

### Hot Reload Implementation

**Signal-based (Unix standard):**
```c
// wlblurd/src/reload.c
void setup_reload_handler(void) {
    struct sigaction sa = {
        .sa_handler = handle_reload,
        .sa_flags = SA_RESTART,
    };
    sigaction(SIGUSR1, &sa, NULL);
}

void handle_reload(int sig) {
    log_info("Reloading configuration");
    
    struct config *new_config = config_load();
    if (!new_config) {
        log_error("Failed to reload config - keeping old");
        return;
    }
    
    config_free(global_config);
    global_config = new_config;
    
    log_info("Configuration reloaded successfully");
}
```

**IPC-based (More control):**
```c
// Add new IPC operation
enum wlblur_op {
    WLBLUR_OP_CREATE_NODE = 1,
    WLBLUR_OP_DESTROY_NODE = 2,
    WLBLUR_OP_RENDER_BLUR = 3,
    WLBLUR_OP_RELOAD_CONFIG = 4,  // NEW
};
```

### State Management

**Approach 1 (Daemon Config):**
```c
struct daemon_state {
    struct config *config;           // Loaded from file
    struct preset *presets;          // Preset definitions
    struct blur_node *nodes;         // Active blur nodes
    uint32_t default_preset_id;      // Which preset is default
};
```

**Approach 2 (IPC-Only):**
```c
struct daemon_state {
    struct blur_node *nodes;         // Active blur nodes only
    // No configuration state
};
```

---

## Performance Implications

### Config File Overhead

**Startup time:**
- Approach 1: +2-5ms (TOML parsing)
- Approach 2: 0ms (no config)

**Memory overhead:**
- Approach 1: +4-8KB (config + presets)
- Approach 2: 0 bytes

**Verdict:** Negligible impact on 1.4ms blur target

### IPC Message Size

**Approach 1 (with presets):**
```c
struct wlblur_request {
    // ... 80 bytes of fixed data ...
    const char preset_name[32];  // "window", "panel", etc.
    // No params needed if using preset
};
// Size: 112 bytes (if using preset)
```

**Approach 2 (full params):**
```c
struct wlblur_request {
    // ... 80 bytes of fixed data ...
    struct wlblur_blur_params params;  // 28 bytes
};
// Size: 108 bytes (always)
```

**Verdict:** Effectively identical

### Caching Implications

**With daemon config:**
- Daemon knows default parameters
- Can cache blurred textures for common settings
- Better cache hit rate

**Without daemon config:**
- Each request potentially unique
- Cache hit rate depends on compositor consistency
- Slightly less predictable

---

## User Experience Analysis

### Scenario 1: Beginner User

**Goal**: Get blur working with minimal effort

**Approach 1 (Daemon Config):**
```bash
# Install wlblur
sudo dnf install wlblur

# Start daemon (uses defaults)
wlblurd &

# Enable in compositor
echo "blur = true" >> ~/.config/scroll/config.toml
```

**Approach 2 (IPC-Only):**
```bash
# Install wlblur
sudo dnf install wlblur

# Start daemon
wlblurd &

# Configure compositor
vim ~/.config/scroll/config.toml
# (must add blur.radius, blur.passes, etc.)
```

**Winner**: Approach 1 (fewer required settings)

### Scenario 2: Power User

**Goal**: Different blur for terminals vs windows vs panels

**Approach 1 (Daemon Config + Presets):**
```toml
# ~/.config/wlblur/config.toml
[presets.terminal]
radius = 3.0
saturation = 1.0

[presets.window]
radius = 8.0
saturation = 1.2

[presets.panel]
radius = 4.0
brightness = 1.1
```

```toml
# ~/.config/scroll/config.toml
[blur]
enabled = true

[[blur.rules]]
app_id = "foot"
preset = "terminal"

[[blur.rules]]
class = "floating"
preset = "window"
```

**Approach 2 (IPC-Only):**
```toml
# ~/.config/scroll/config.toml
[blur]
enabled = true

[[blur.rules]]
app_id = "foot"
radius = 3.0
saturation = 1.0

[[blur.rules]]
class = "floating"
radius = 8.0
saturation = 1.2
```

**Winner**: Approach 1 (reusable presets, DRY)

### Scenario 3: Multi-Compositor User

**Goal**: Consistent blur across 3 compositors

**Approach 1:**
```bash
# Single source of truth
~/.config/wlblur/config.toml

# Each compositor just enables blur
scroll:  blur = true
niri:    blur { enabled true }
sway:    blur_enable yes
```

**Approach 2:**
```bash
# Must duplicate settings
~/.config/scroll/config.toml
~/.config/niri/config.kdl
~/.config/sway/config

# Keep manually in sync
```

**Winner**: Approach 1 (single source of truth)

---

## Maintainability Analysis

### Daemon Complexity

**Approach 1:**
- Config parsing code (+400 lines)
- Preset management (+200 lines)
- Hot reload logic (+100 lines)
- **Total**: +700 lines

**Approach 2:**
- No additional code
- **Total**: 0 lines

**Verdict**: Approach 2 simpler for daemon maintainers

### Compositor Integration Complexity

**Approach 1:**
```c
// compositor integration (minimal)
struct wlblur_request req = {
    .use_defaults = true,  // Just use daemon config
    // or
    .preset_name = "window",
};
```

**Approach 2:**
```c
// compositor integration (must provide all params)
struct wlblur_request req = {
    .params = {
        .num_passes = config->blur_passes,
        .radius = config->blur_radius,
        .brightness = config->blur_brightness,
        // ... all 7 parameters ...
    },
};
```

**Verdict**: Approach 1 simpler for compositor developers

### Testing Complexity

**Approach 1:**
- Must test config parsing
- Must test preset resolution
- Must test hot reload
- Must test fallback hierarchy
- **Test cases**: +50

**Approach 2:**
- Just test IPC protocol
- **Test cases**: existing

**Verdict**: Approach 2 easier to test

---

## Recommendations

### For Discussion

**Questions to explore:**

1. **Primary use case priority?**
   - Single compositor user (Approach 2 fine)
   - Multi-compositor user (Approach 1 better)
   - Compositor developer (Approach 1 easier integration)

2. **Configuration complexity tolerance?**
   - Simple users want defaults (Approach 1)
   - Power users want full control (Approach 2)
   - Hybrid serves both (Approach 3)

3. **Maintenance burden ownership?**
   - Daemon maintainer complexity (Approach 1)
   - Compositor maintainer complexity (Approach 2)
   - Shared complexity (Approach 3)

4. **Ecosystem alignment?**
   - Follow kanshi/mako pattern (Approach 1)
   - Follow minimal Unix tool pattern (Approach 2)
   - Provide both options (Approach 3)

5. **Future extensibility?**
   - Material presets (needs Approach 1)
   - Algorithm selection (needs Approach 1)
   - Dynamic adaptation (works either way)

### Hybrid Recommendation

**Suggested implementation:**

```c
// Daemon config file (optional)
~/.config/wlblur/config.toml  // If exists, provides defaults

// IPC protocol supports both
struct wlblur_request {
    // If compositor sends params, use them
    // If compositor sends NULL, use daemon defaults
    // If no daemon config, use hardcoded defaults
    struct wlblur_blur_params *params;  // NULL = use defaults
};
```

**Benefits:**
- ✅ Works with no config (hardcoded defaults)
- ✅ Users can add config for convenience
- ✅ Compositors can override when needed
- ✅ Multi-compositor consistency possible
- ✅ Gradual adoption path

**Implementation strategy:**
1. Ship m-3 with Approach 2 (IPC-only, simple)
2. Add Approach 1 features in m-4+ (config file, presets)
3. Maintain backward compatibility (optional config)

---

## Questions for Deep Discussion

1. **How important is hot-reload to users?**
   - Can they restart compositor instead?
   - Is live experimentation critical?

2. **Should presets be standardized across compositors?**
   - "window", "panel", "hud" - everyone uses same names?
   - Or compositor-specific?

3. **Who owns the "blur aesthetic"?**
   - Compositor (part of its identity)?
   - User (personal preference)?
   - Daemon (consistent system-wide)?

4. **What about future material system?**
   - Material presets definitely need daemon config
   - But that's m-7, months away
   - YAGNI vs future-proofing?

5. **Configuration discovery UX?**
   - How do users learn about config options?
   - Man pages? GUI? Example files?

6. **Environment variables vs config file?**
   ```bash
   WLBLUR_RADIUS=8.0 wlblurd
   ```
   - More Unix-y?
   - Less discoverable?

---

## Decision Framework

**Choose Approach 1 if:**
- ✅ Multi-compositor consistency is high priority
- ✅ Future material system is planned soon
- ✅ Want to minimize compositor integration effort
- ✅ Users will want to experiment with settings

**Choose Approach 2 if:**
- ✅ Daemon simplicity is paramount
- ✅ Only one compositor will integrate initially
- ✅ Compositor developers want full control
- ✅ Want to ship m-3 fastest

**Choose Approach 3 if:**
- ✅ Want to satisfy both use cases
- ✅ Willing to maintain slightly more complexity
- ✅ Expect diverse user preferences
- ✅ Want gradual adoption path

---

**This write-up provides complete context for an in-depth discussion about configuration architecture. The next LLM should analyze tradeoffs, consider user needs, examine precedents, and make a recommendation for which approach to implement in m-3.**

---

User replied:
crucial consideration: i want to use quickshell (qmp shell for wayland) for most of my desktop operations. these are typically configured as layershell specfic (this is how hyprland sets different blur levels for specific surfaces).

in my case i m contemplating to leverage  ext_background_effect_v1 (docs below)

```
Protocol ext_background_effect_v1
License
Index
ext_background_effect_manager_v1
destroy
get_background_effect
capabilities
error
capability
ext_background_effect_surface_v1
destroy
set_blur_region
error
Interface ext_background_effect_manager_v1 — background effect factory
This protocol provides a way to improve visuals of translucent surfaces by applying effects like blur to the background behind them.

The capabilities are send when the global is bound, and every time they change. Note that when the capability goes away, the corresponding effect is no longer applied by the compositor, even if it was set before.

Warning! The protocol described in this file is currently in the testing phase. Backward compatible changes may be added together with the corresponding interface version bump. Backward incompatible changes can only be done by creating a new major version of the extension.

Request ext_background_effect_manager_v1.destroy — destroy the background effect manager
Informs the server that the client will no longer be using this protocol object. Existing objects created by this object are not affected.

Request ext_background_effect_manager_v1.get_background_effect — get a background effects object
Instantiate an interface extension for the given wl_surface to add effects like blur for the background behind it.

If the given wl_surface already has a ext_background_effect_surface_v1 object associated, the background_effect_exists protocol error will be raised.

id new_id<ext_background_effect_surface_v1>: the new ext_background_effect_surface_v1 object
surface object<wl_surface>: the surface
Event ext_background_effect_manager_v1.capabilities — capabilities of the compositor
flags uint: None
Enum ext_background_effect_manager_v1.error
background_effect_exists: the surface already has a background effect object
Enum ext_background_effect_manager_v1.capability
blur: the compositor supports applying blur
Interface ext_background_effect_surface_v1 — background effects for a surface
The background effect object provides a way to specify a region behind a surface that should have background effects like blur applied.

If the wl_surface associated with the ext_background_effect_surface_v1 object has been destroyed, this object becomes inert.

Request ext_background_effect_surface_v1.destroy — release the blur object
Informs the server that the client will no longer be using this protocol object. The effect regions will be removed on the next commit.

Request ext_background_effect_surface_v1.set_blur_region — set blur region
This request sets the region of the surface that will have its background blurred.

The blur region is specified in the surface-local coordinates, and clipped by the compositor to the surface size.

The initial value for the blur region is empty. Setting the pending blur region has copy semantics, and the wl_region object can be destroyed immediately. A NULL wl_region removes the effect.

The blur region is double-buffered state, and will be applied on the next wl_surface.commit.

The blur algorithm is subject to compositor policies.

If the associated surface has been destroyed, the surface_destroyed error will be raised.

region object<wl_region>: blur region of the surface
Enum ext_background_effect_surface_v1.error
surface_destroyed: the associated surface has been destroyed
```

this means that the blur itself is requested by the item "hey compositor, blur me here please"; but this does not include "hey compositor, blur me here wit hthose settings, and there with these settings"). it is expected for that to be done on the compositor config. 

but in this new, blur-specific paradign the compositor requests blur from IPC (yes, as far as the windowed program/layershell is concerned this IS handled by the compositor) but i want to navigate that.
the idea or presets is especially appealing to me because of its potential compatibility. finally there is pull here because wlblur will be upgrading its blur algorithms (adding more macos bling soon for instance like tint and vibrancy).

and yes: minimizing compositor effort is desired.

---

Claude said:
# CRITICAL ADDITION: ext_background_effect_v1 Protocol Integration

## The Missing Piece

This changes EVERYTHING about the configuration architecture discussion. Let me explain why.

---

## The Protocol Reality

### What ext_background_effect_v1 Actually Does

From the protocol spec:

```c
// Client (quickshell) says:
ext_background_effect_manager_v1.get_background_effect(surface)
ext_background_effect_surface_v1.set_blur_region(region)

// That's it. No parameters. Just "blur me here please"
```

**Key limitation**: The protocol does NOT include:
- Blur radius
- Number of passes
- Saturation, brightness, contrast
- Any visual parameters

**Protocol philosophy**: 
> "The blur algorithm is subject to compositor policies."

Translation: **Compositor decides how to blur.**

---

## The Three-Layer Stack

With wlblur, we have a NEW architecture:

```
┌─────────────────────────────────────────┐
│  Layer 1: Client Application            │
│  (quickshell, waybar, etc.)             │
│                                         │
│  Uses: ext_background_effect_v1         │
│  Says: "I want blur"                    │
│  Doesn't say: HOW to blur               │
└─────────────────────────────────────────┘
              ↓ Wayland protocol
┌─────────────────────────────────────────┐
│  Layer 2: Compositor                    │
│  (scroll, niri, sway)                   │
│                                         │
│  Receives: Blur request from client     │
│  Must decide: WHICH blur settings?      │
│  Forwards to: wlblur daemon             │
└─────────────────────────────────────────┘
              ↓ Unix socket + DMA-BUF
┌─────────────────────────────────────────┐
│  Layer 3: wlblur Daemon                 │
│                                         │
│  Receives: Blur request + parameters    │
│  Applies: Actual blur rendering         │
│  Returns: Blurred texture               │
└─────────────────────────────────────────┘
```

**The question**: When compositor receives blur request from client, what parameters does it send to wlblur?

---

## Current Hyprland Approach (For Comparison)

Hyprland handles this with **layershell-specific blur rules**:

```ini
# hyprland.conf
blur {
    enabled = true
    size = 8
    passes = 3
}

# Different blur for different surface types
layerrule = blur, waybar
layerrule = blur, rofi
blurls = gtk-layer-shell  # All gtk layershell apps

# Can even set per-surface blur strength
layerrule = blur:1.5, waybar  # Extra strong blur for waybar
```

**How it works internally:**
1. Waybar requests blur via ext_background_effect_v1
2. Hyprland sees it's a layershell surface named "waybar"
3. Hyprland looks up blur rules: "waybar gets 1.5x blur"
4. Hyprland applies blur with adjusted parameters

**Problem with this for wlblur**: 
- Compositor must maintain ALL the blur logic
- Compositor config must specify all parameters
- When wlblur adds tint/vibrancy, compositor config must be updated
- This is exactly the maintenance burden we're trying to avoid!

---

## The Preset Solution

### How Presets Solve This

**wlblur daemon config:**
```toml
# ~/.config/wlblur/config.toml

[presets.panel]
# For desktop panels (waybar, quickshell panels)
num_passes = 2
radius = 4.0
saturation = 1.1
brightness = 1.05

[presets.window]
# For regular application windows
num_passes = 3
radius = 8.0
saturation = 1.15
brightness = 1.0

[presets.hud]
# For overlay/HUD elements (rofi, launcher)
num_passes = 4
radius = 12.0
saturation = 1.2
vibrancy = 0.2

[presets.tooltip]
# For tooltips and small popups
num_passes = 1
radius = 2.0
saturation = 1.0
```

**Compositor integration (minimal):**
```c
// scroll/src/blur_integration.c

const char* get_blur_preset_for_surface(struct wl_surface *surface) {
    // Simple surface type detection
    if (is_layershell_surface(surface)) {
        const char *namespace = get_layershell_namespace(surface);
        
        if (strcmp(namespace, "waybar") == 0) return "panel";
        if (strcmp(namespace, "quickshell") == 0) return "panel";
        if (strcmp(namespace, "rofi") == 0) return "hud";
        
        return "panel";  // Default for layershell
    }
    
    if (is_xdg_popup(surface)) {
        return "tooltip";
    }
    
    return "window";  // Default for regular windows
}

// When handling ext_background_effect_v1 request:
void handle_blur_request(struct wl_surface *surface, struct wl_region *region) {
    const char *preset = get_blur_preset_for_surface(surface);
    
    // Send to wlblur daemon
    struct wlblur_request req = {
        .op = WLBLUR_OP_RENDER_BLUR,
        .preset_name = preset,  // Just pass preset name!
        // No need to specify all parameters
    };
    
    send_blur_request(&req);
}
```

**User configuration (compositor config):**
```toml
# ~/.config/scroll/config.toml

[blur]
enabled = true
daemon_socket = "/run/user/1000/wlblur.sock"

# Optional: Override preset mappings
[blur.preset_mappings]
waybar = "panel"
quickshell = "panel"
rofi = "hud"
foot = "window"  # Terminal gets window blur
```

---

## Why This Is The Right Approach

### 1. Minimal Compositor Effort ✅

**What compositor needs to implement:**
- Surface type detection (~30 lines)
- Preset name lookup (~20 lines)
- IPC request with preset name (~10 lines)

**What compositor does NOT need:**
- Blur parameter configuration
- Blur algorithm knowledge
- Visual tuning logic
- Future feature updates (tint, vibrancy, materials)

**Total compositor code**: Still ~220 lines as promised

### 2. Future-Proof for Tint/Vibrancy ✅

**When wlblur adds tint (m-6):**

```toml
# User just updates wlblur config
[presets.panel]
radius = 4.0
tint_color = [242, 242, 247, 230]  # NEW: macOS-style tint
tint_strength = 0.9
```

**Compositor changes needed**: ZERO

**When wlblur adds vibrancy (m-7):**

```toml
[presets.hud]
radius = 12.0
vibrancy_strength = 0.2  # NEW: HSL saturation boost
vibrancy_darkness = true
```

**Compositor changes needed**: ZERO

**When wlblur adds materials (m-8):**

```toml
[presets.panel]
material = "sidebar"  # NEW: Use macOS material system
```

**Compositor changes needed**: ZERO

### 3. Cross-Compositor Consistency ✅

**User setup:**
```bash
# Single config for all compositors
~/.config/wlblur/config.toml

[presets.panel]
radius = 4.0
saturation = 1.1
```

**Result:**
- Waybar looks identical in scroll, niri, and sway
- Quickshell panels look identical across all compositors
- User configures once, works everywhere

### 4. User Customization ✅

**User can customize per-compositor if needed:**

```toml
# ~/.config/scroll/config.toml
[blur.preset_mappings]
quickshell = "panel_bright"  # Use different preset in scroll

# ~/.config/niri/config.kdl
blur {
    preset-mappings {
        quickshell = "panel"  # Use standard preset in niri
    }
}
```

**But more commonly, they just use the daemon presets**

---

## Modified IPC Protocol

### Extended Request Structure

```c
struct wlblur_request {
    uint32_t protocol_version;
    uint32_t op;
    uint32_t node_id;
    
    // Texture info
    uint32_t width;
    uint32_t height;
    uint32_t format;
    uint64_t modifier;
    uint32_t stride;
    uint32_t offset;
    
    // NEW: Preset specification
    char preset_name[32];        // "panel", "window", "hud", etc.
    bool use_preset;             // true = use preset, false = use params
    
    // Optional: Direct parameter override
    struct wlblur_blur_params params;  // Only used if use_preset = false
};
```

### Preset Resolution Algorithm

```c
// wlblurd/src/presets.c

struct wlblur_blur_params* resolve_preset(
    const char *preset_name,
    const struct wlblur_blur_params *override_params
) {
    // 1. Try named preset
    if (preset_name && preset_name[0] != '\0') {
        struct preset *p = preset_lookup(preset_name);
        if (p) {
            return &p->params;
        }
        log_warn("Preset '%s' not found, using default", preset_name);
    }
    
    // 2. Try direct parameter override
    if (override_params) {
        return override_params;
    }
    
    // 3. Fall back to daemon defaults
    return &default_params;
}
```

**Fallback hierarchy:**
1. Named preset (if found in daemon config)
2. Direct parameters (if compositor provides)
3. Daemon defaults (hardcoded or from config)

---

## Configuration Examples

### Minimal User (Just Works)

**User does nothing**, wlblur ships with sensible defaults:

```toml
# Built-in defaults (hardcoded in daemon)
[presets.window]
radius = 5.0
passes = 3

[presets.panel]
radius = 4.0
passes = 2

[presets.hud]
radius = 8.0
passes = 3
```

**Compositor integration:**
```c
// Just map surface types to preset names
preset = is_panel(surface) ? "panel" : "window";
```

**Result**: Blur works, looks decent, zero config needed

---

### Power User (Customized)

**User creates config:**

```toml
# ~/.config/wlblur/config.toml

[daemon]
socket_path = "/run/user/1000/wlblur.sock"

[defaults]
# System-wide defaults (used by new presets)
num_passes = 3
radius = 5.0
saturation = 1.1

[presets.panel]
num_passes = 2
radius = 4.0
brightness = 1.05
saturation = 1.1

[presets.window]
num_passes = 3
radius = 8.0
saturation = 1.15

[presets.terminal]
# Special preset for terminals (less blur)
num_passes = 2
radius = 3.0
saturation = 1.0

[presets.quickshell_panel]
# Custom preset for quickshell panels specifically
num_passes = 2
radius = 4.5
brightness = 1.1
saturation = 1.2
noise = 0.01
```

**Compositor mapping:**
```toml
# ~/.config/scroll/config.toml
[blur.preset_mappings]
quickshell = "quickshell_panel"  # Use custom preset
foot = "terminal"                # Terminal-specific blur
kitty = "terminal"
alacritty = "terminal"
```

**Result**: Fully customized blur per application type

---

### Multi-Compositor User

**Single source of truth:**

```toml
# ~/.config/wlblur/config.toml
[presets.panel]
radius = 4.0
saturation = 1.1
```

**All compositors:**
```c
// scroll, niri, sway all do the same thing:
preset = is_panel(surface) ? "panel" : "window";
```

**Result**: Consistent appearance everywhere

---

## Quickshell-Specific Workflow

### Your Use Case

**Quickshell panel configuration:**
```qml
// quickshell/panel.qml
ShellLayer {
    layer: Layer.Top
    
    Rectangle {
        // Quickshell requests blur via ext_background_effect_v1
        color: "transparent"
        backgroundBlur: true  // This triggers the protocol
        
        // No blur parameters here - they're in wlblur config!
    }
}
```

**wlblur config:**
```toml
# ~/.config/wlblur/config.toml
[presets.panel]
# Your preferred panel blur
radius = 4.5
brightness = 1.1
saturation = 1.2
noise = 0.01

# Future: Add tint for macOS-style panels
# tint_color = [242, 242, 247, 230]
# tint_strength = 0.9
```

**Compositor (scroll) integration:**
```c
// scroll automatically detects quickshell layershell
const char *namespace = layershell_get_namespace(surface);
if (strcmp(namespace, "quickshell") == 0) {
    preset = "panel";  // Use panel preset from wlblur config
}
```

**Benefits for you:**
- Configure blur once in wlblur config
- Works across scroll, niri, sway (when you switch)
- When wlblur adds tint/vibrancy, just update config
- Quickshell QML files don't need blur parameters
- Standard ext_background_effect_v1 protocol

---

## Future Evolution Path

### Phase 1 (m-3): Basic Presets

```toml
[presets.window]
radius = 5.0
passes = 3

[presets.panel]
radius = 4.0
passes = 2
```

**Compositor integration**: ~220 lines

---

### Phase 2 (m-6): Add Tint

```toml
[presets.panel]
radius = 4.0
passes = 2
tint_color = [242, 242, 247, 230]  # NEW
tint_strength = 0.9                 # NEW
```

**Compositor changes**: 0 lines  
**User changes**: Edit wlblur config

---

### Phase 3 (m-7): Add Vibrancy

```toml
[presets.hud]
radius = 8.0
passes = 3
vibrancy_strength = 0.2    # NEW
vibrancy_darkness = true   # NEW
```

**Compositor changes**: 0 lines  
**User changes**: Edit wlblur config

---

### Phase 4 (m-8): Material System

```toml
[presets.panel]
material = "sidebar"  # NEW: Reference macOS material
# This expands to full parameter set automatically
```

**Compositor changes**: 0 lines  
**User changes**: Edit wlblur config

---

## The Decision Is Clear

Given your requirements:

1. ✅ **Quickshell usage** → ext_background_effect_v1 doesn't include parameters
2. ✅ **Different blur per surface** → Presets handle this perfectly
3. ✅ **Future tint/vibrancy** → Presets evolve without compositor changes
4. ✅ **Minimize compositor effort** → Compositor just maps types to preset names
5. ✅ **Multi-compositor consistency** → Single wlblur config

**You MUST use Approach 1 (Daemon Config with Presets)**

---

## Implementation Strategy

### For m-3 (ScrollWM Integration)

**Phase 1: Ship with hardcoded presets**
```c
// wlblurd/src/presets.c
static const struct preset builtin_presets[] = {
    {"window", {.radius = 5.0, .passes = 3}},
    {"panel", {.radius = 4.0, .passes = 2}},
    {"hud", {.radius = 8.0, .passes = 3}},
};
```

**Phase 2: Add config file support (m-4)**
```c
// Read ~/.config/wlblur/config.toml
// Override builtin presets
// Add user-defined presets
```

**Benefits:**
- m-3 ships faster (no config parsing yet)
- Proves the preset concept works
- Config file can be added later without breaking anything

---

## Recommended Next Steps

1. **Accept Approach 1** (Daemon Config with Presets)
2. **Define standard preset names** (window, panel, hud, tooltip)
3. **Implement preset system in m-3** (hardcoded initially)
4. **Add config file in m-4** (TOML parsing, hot reload)
5. **Document preset usage** for compositor integrators

---

## Final Answer

**For the configuration architecture discussion:**

**MUST HAVE**: Daemon presets (Approach 1)

**Rationale:**
- ext_background_effect_v1 doesn't include parameters
- Compositor must decide parameters when forwarding to wlblur
- Presets minimize compositor code (just map surface type → preset name)
- Future features (tint, vibrancy, materials) require preset evolution
- Multi-compositor consistency requires single configuration source
- Your quickshell workflow explicitly needs this

**Implementation:**
- Start with hardcoded presets in m-3
- Add config file in m-4
- Support hot reload (SIGUSR1)
- Use TOML format

This is not a "nice to have" - it's architecturally required given your use case.

---

User said:
clearly we must swap milestones order: m-4 (config parsing) before m-3 (scroll window manager full implementation).
the decisions made above must also be reflected in the docs under decision variables (the same way i documented the decision to move to an external ipc for instance).
this will be handed off to another dedicated claude session.
a coding llm found by going through the exsisting repo:

```

Project Snapshot
The Milestone m-2 completion report confirms that foundational work spanning documentation, the libwlblur library, and the wlblurd daemon is finished with high quality; it enumerates code volume, shader assets, and build readiness, demonstrating a mature codebase poised for integration efforts.

The architecture overview positions wlblur as a compositor-agnostic blur service where lightweight IPC clients send DMA-BUF-backed textures to the wlblurd daemon, which orchestrates libwlblur for GPU-accelerated dual-Kawase blurring before returning results via Unix sockets.

Post–Milestone 2 Discussion Insights
A new architectural question has emerged: whether blur configuration should live in a dedicated daemon-side file or remain embedded in each compositor’s config, with suggestions to support wlblurd startup flags and daemon hot-reload to enable rapid experimentation.

Addressing this requires attention to the unimplemented configuration module (wlblurd/src/config.c), which is currently just a placeholder and will be central to any decision about standalone configuration management and live reloadability.

Key Implementation Touchpoints for the Architecture Agent
Public library API: libwlblur/include/wlblur/wlblur.h defines the exported surface (context lifecycle, blur entry point, error handling, versioning) that compositors and the daemon rely on, clarifying what configuration surfaces might need to be exposed or parameterized.

Core blur pipeline: libwlblur/src/blur_context.c wires EGL setup, parameter validation, DMA-BUF import/export, and the dual-Kawase renderer, highlighting where configuration toggles (passes, radius, effects) are consumed today.

DMA-BUF interop: libwlblur/src/dmabuf.c encapsulates cross-plane imports and exports with modifier support, useful when evaluating format compatibility or hot-reload effects on resource lifetimes.

Daemon entry & lifecycle: wlblurd/src/main.c shows socket setup, signal handling, and the epoll loop that would host any config reload triggers or command-line parsing enhancements.

IPC protocol handling: wlblurd/src/ipc_protocol.c and wlblurd/include/protocol.h implement request/response marshaling, status codes, and the global blur context; these files determine how configuration choices propagate over the wire.

Client and node registries: wlblurd/src/client.c and wlblurd/src/blur_node.c manage per-compositor state, resource quotas, and lifecycle cleanup, which are likely touchpoints if configuration affects per-node defaults or runtime adjustments.

These references should equip the advanced architecture agent with the necessary context to evaluate configuration strategy changes alongside the existing daemon-centric blur workflow.
```

---

# Handoff Document: Configuration Architecture Decision & ADR Creation

## Mission

Document the **configuration architecture decision** for wlblur in the same style and rigor as existing ADRs (Architecture Decision Records). This decision fundamentally shapes how blur parameters flow from users → compositors → daemon.

## Critical Context

### The Decision

**DECISION**: Implement **Approach 1 - Daemon Configuration with Presets**

wlblur will use a daemon-side configuration file (`~/.config/wlblur/config.toml`) with named presets that compositors reference by name, rather than compositors sending full parameters with every request.

### Why This Decision Is Architecturally Required

**The ext_background_effect_v1 Protocol Constraint:**
```c
// Protocol ONLY allows:
ext_background_effect_surface_v1.set_blur_region(region)

// Does NOT allow:
set_blur_region(region, radius, passes, saturation, ...)
```

**The three-layer flow:**
1. **Client (quickshell/waybar)** → uses ext_background_effect_v1 → says "blur me" (no parameters)
2. **Compositor (scroll/niri)** → receives blur request → must decide WHICH parameters
3. **Daemon (wlblurd)** → receives parameters from compositor → applies blur

**The problem**: If compositor must specify all parameters, then:
- Compositor config gets bloated with blur details
- Future features (tint, vibrancy, materials) require compositor config updates
- Multi-compositor users must duplicate configs
- This defeats the "minimal compositor integration" goal

**The solution**: Compositor just says "use preset: panel" and daemon resolves parameters.

---

## Milestone Reordering

**OLD order:**
- m-3: ScrollWM Integration
- m-4: Testing & Validation

**NEW order:**
- **m-3**: Configuration System Implementation (config parsing, presets, hot reload)
- **m-4**: ScrollWM Integration (now can reference presets)
- **m-5**: Testing & Validation

**Rationale**: ScrollWM integration requires preset system to be functional.

---

## Files to Provide

Provide the coding LLM with these files from the repository:

### Existing ADRs (for style reference)
1. **`docs/decisions/001-why-external-daemon.md`** - Shows ADR format and depth
2. **`docs/decisions/002-dma-buf-vs-alternatives.md`** - Shows technical comparison style
3. **`docs/decisions/004-ipc-protocol-design.md`** - Shows protocol design rationale

### Implementation Context
4. **`wlblurd/src/config.c`** - Currently placeholder, will be implemented
5. **`wlblurd/include/protocol.h`** - IPC protocol (will be extended with presets)
6. **`wlblurd/src/main.c`** - Daemon lifecycle (shows signal handling location)
7. **`libwlblur/include/wlblur/blur_params.h`** - Parameter structure

### Background Documentation
8. **`docs/post-investigation/blur-daemon-approach.md`** - Daemon rationale
9. **`milestone2-report.md`** - Current project state
10. **`ROADMAP.md`** - Will need updating for milestone reorder

### Discussion Context
11. **The comprehensive configuration discussion document** (from this conversation)

---

## Deliverables Expected

### Primary Deliverable: ADR 006

**File**: `docs/decisions/006-daemon-configuration-with-presets.md`

**Required sections** (following existing ADR format):

```markdown
# ADR 006: Daemon Configuration with Presets

## Status
Accepted

## Context
[Explain the configuration architecture question]
[Include ext_background_effect_v1 protocol constraint]
[Describe three-layer flow: client → compositor → daemon]

## Decision
[State: Daemon-side configuration with named presets]
[Explain preset resolution mechanism]

## Alternatives Considered

### Alternative 1: IPC-Only Configuration
[Pros/cons of compositor-provided parameters]

### Alternative 2: Hybrid Approach
[Pros/cons of optional daemon config]

### Alternative 3: Per-Compositor Configuration
[Pros/cons of duplicate configs]

## Rationale

### ext_background_effect_v1 Protocol Constraint
[Protocol doesn't include parameters]
[Compositor must decide parameters when forwarding]

### Minimal Compositor Integration Goal
[220-line integration promise]
[Presets reduce compositor code]

### Future Feature Evolution
[Tint, vibrancy, materials coming in m-6, m-7, m-8]
[Presets enable zero-compositor-change upgrades]

### Multi-Compositor Consistency
[Single source of truth for users]
[Quickshell use case specifically]

## Consequences

### Positive
- Compositor integration remains ~220 lines
- Future features (tint, vibrancy) require no compositor changes
- Multi-compositor users configure once
- Preset system enables material system (m-8)
- Hot reload enables experimentation

### Negative
- Daemon complexity increases (+700 lines)
- Config parsing dependency (TOML library)
- Testing complexity increases
- Additional documentation burden

### Neutral
- Preset names must be standardized
- Migration path needed for existing configs (if any)

## Implementation Notes

### Configuration File Format
[TOML chosen - explain why]

### Standard Preset Names
- `window` - Regular application windows
- `panel` - Desktop panels (waybar, quickshell)
- `hud` - Overlay elements (rofi, launchers)
- `tooltip` - Small popups

### Hot Reload Mechanism
[SIGUSR1 signal handler]
[Config validation before applying]

### Fallback Hierarchy
1. Named preset (if found)
2. Direct parameters (if provided)
3. Daemon defaults (hardcoded)

## Related Decisions
- ADR 001: Why External Daemon (established need for daemon)
- ADR 004: IPC Protocol Design (extended with preset support)

## References
- ext_background_effect_v1 protocol specification
- Hyprland layerrule configuration pattern
- kanshi/mako configuration precedents
```

---

### Secondary Deliverable: Updated Milestone Definitions

**Files to update:**

**A. `backlog/milestones/m-3.md`**

Change from "ScrollWM Integration" to "Configuration System Implementation"

```markdown
# Milestone m-3: Configuration System Implementation

**Timeline**: Weeks 6-8
**Status**: Next
**Depends on**: m-2

## Objectives
Implement daemon-side configuration system with preset support,
enabling minimal compositor integration while supporting future
feature evolution (tint, vibrancy, materials).

## Deliverables
- [ ] TOML config file parsing
- [ ] Preset definition and resolution
- [ ] Hot reload via SIGUSR1
- [ ] Extended IPC protocol with preset support
- [ ] Configuration validation
- [ ] Default presets (window, panel, hud, tooltip)
- [ ] Config file documentation
- [ ] Migration guide for milestone reordering

## Rationale
Configuration system must exist before compositor integration
because ext_background_effect_v1 protocol doesn't include blur
parameters. Compositors need to reference presets by name.

See: ADR 006 - Daemon Configuration with Presets
```

**B. `backlog/milestones/m-4.md`**

Change from "Testing & Validation" to "ScrollWM Integration"

```markdown
# Milestone m-4: ScrollWM Integration

**Timeline**: Weeks 9-10
**Status**: Planned
**Depends on**: m-3 (Configuration System)

## Objectives
Create ~220-line blur integration module for ScrollWM that
demonstrates the preset-based architecture works with real
compositor constraints.

## Deliverables
- [ ] ScrollWM blur integration module (~220 lines)
- [ ] Surface type detection (layershell, xdg_surface, etc.)
- [ ] Preset name mapping logic
- [ ] ext_background_effect_v1 protocol handling
- [ ] DMA-BUF export for backdrop rendering
- [ ] IPC client for wlblurd communication
- [ ] Integration testing with quickshell panels
- [ ] Performance benchmarking (target: <1.5ms total)
- [ ] Integration documentation for other compositors

## Success Metrics
- Integration code ≤ 250 lines
- Works with quickshell layershell surfaces
- Preset resolution functional
- No compositor restart needed for preset changes
- Performance within target (<1.5ms)
```

**C. `backlog/milestones/m-5.md`**

Rename from m-4, update dependencies

```markdown
# Milestone m-5: Testing & Validation

**Timeline**: Weeks 11-12
**Status**: Planned
**Depends on**: m-4 (ScrollWM Integration)

[Rest of content unchanged]
```

---

### Tertiary Deliverable: Implementation Specifications

**File**: `docs/architecture/04-configuration-system.md` (NEW)

```markdown
# Configuration System Architecture

## Overview
[High-level description of preset-based configuration]

## Configuration File Format

### Location
`~/.config/wlblur/config.toml`

### Schema
[Complete TOML schema with examples]

### Validation Rules
[Parameter ranges, required fields, etc.]

## Preset Resolution

### Algorithm
[Flowchart: preset name → parameters → fallback]

### Standard Presets
[Definition of window, panel, hud, tooltip]

## Hot Reload

### Mechanism
[SIGUSR1 signal handling]

### Validation
[Config parsing and validation before applying]

### Error Handling
[What happens if reload fails]

## IPC Protocol Extension

### Modified Request Structure
[Show extended wlblur_request with preset_name field]

### Backward Compatibility
[How old compositors continue working]

## Implementation Files

- `wlblurd/src/config.c` - TOML parsing
- `wlblurd/src/presets.c` - Preset management
- `wlblurd/src/reload.c` - Hot reload logic
- `wlblurd/include/config.h` - Config structures
```

---

### Additional Updates Needed

**Update `ROADMAP.md`:**
- Change m-3 from "wlblurd IPC Daemon" to "Configuration System"
- Change m-4 from "First Integration" to "ScrollWM Integration"
- Update timeline estimates
- Add note about milestone reordering rationale

**Update `README.md`:**
- Project Status section: Note m-3 is next (configuration)
- Quick Start section: Show example config file usage
- Add Configuration section explaining preset system

**Create `docs/configuration-guide.md`:**
- User-facing documentation
- Example configurations
- Preset customization guide
- Hot reload instructions
- Troubleshooting common config errors

---

## Technical Specifications for Implementation

### Dependencies

**TOML Parsing Library:**
- **tomlc99** (recommended) - Single header, MIT license
- Alternative: **toml++** (C++17, header-only)
- Alternative: **ctoml** (minimal, pure C)

**Recommendation**: Use tomlc99
- Lightweight (~2000 lines)
- Well-tested
- MIT license (compatible)
- Used by other Wayland tools

### Code Volume Estimate

**New files:**
```
wlblurd/src/config.c          ~300 lines (TOML parsing)
wlblurd/src/presets.c         ~200 lines (Preset management)
wlblurd/src/reload.c          ~100 lines (Hot reload)
wlblurd/include/config.h      ~150 lines (Structures)

Total: ~750 lines
```

**Modified files:**
```
wlblurd/src/main.c            +50 lines (Signal setup)
wlblurd/src/ipc_protocol.c    +80 lines (Preset resolution)
wlblurd/include/protocol.h    +20 lines (Extended request)

Total modifications: ~150 lines
```

**Grand total**: ~900 lines for configuration system

### Configuration Structure

```c
// wlblurd/include/config.h

struct daemon_config {
    // Daemon settings
    char socket_path[256];
    char log_level[16];
    uint32_t max_nodes_per_client;
    
    // Default parameters
    struct wlblur_blur_params defaults;
    
    // Presets
    struct preset *presets;      // Linked list
    size_t preset_count;
};

struct preset {
    char name[32];                    // "window", "panel", etc.
    struct wlblur_blur_params params;
    struct preset *next;
};

// API
struct daemon_config* config_load(const char *path);
void config_free(struct daemon_config *config);
bool config_validate(const struct daemon_config *config);
struct preset* config_get_preset(const struct daemon_config *config,
                                  const char *name);
```

---

## Questions for the Coding LLM

The ADR should address these questions explicitly:

1. **Why not use environment variables instead of config file?**
   - Less discoverable
   - Harder to manage multiple presets
   - Not standard for Wayland tools

2. **Why TOML over JSON/INI?**
   - Human-readable with comments
   - Typed values
   - Standard in Wayland ecosystem
   - Good library support

3. **What happens if config file doesn't exist?**
   - Use hardcoded defaults
   - Document default preset values
   - Daemon works out-of-box

4. **What happens if config file is invalid?**
   - Log error
   - Fall back to previous config (hot reload)
   - Fall back to defaults (startup)
   - Don't crash daemon

5. **How does this affect compositor integration?**
   - Simplifies integration (just preset names)
   - Reduces compositor code
   - Enables future features

6. **Migration path for future changes?**
   - Version field in config file
   - Backward compatibility guarantees
   - Deprecation warnings for old formats

---

## Success Criteria

The deliverables are complete when:

### ADR Quality
- [ ] Follows format of existing ADRs (001-005)
- [ ] Explains ext_background_effect_v1 constraint clearly
- [ ] Compares all three approaches (IPC-only, Daemon config, Hybrid)
- [ ] Provides clear rationale for decision
- [ ] Documents consequences (positive, negative, neutral)
- [ ] Includes implementation notes
- [ ] References related decisions and protocols

### Milestone Updates
- [ ] m-3 redefined as Configuration System
- [ ] m-4 redefined as ScrollWM Integration
- [ ] m-5 timeline adjusted
- [ ] Dependencies updated correctly
- [ ] Rationale for reordering documented

### Architecture Documentation
- [ ] Configuration system architecture documented
- [ ] Preset resolution algorithm specified
- [ ] Hot reload mechanism explained
- [ ] IPC protocol extension defined
- [ ] Implementation files identified

### ROADMAP Updates
- [ ] Milestone table reflects new order
- [ ] Timeline estimates updated
- [ ] Milestone reordering rationale added
- [ ] No broken links or references

---

## Writing Style Guidelines

**Follow the style of ADR 001-005:**

- **Formal but readable** - Technical precision without jargon
- **Decision-focused** - Center on the choice being made
- **Evidence-based** - Back claims with technical rationale
- **Comprehensive** - Consider all reasonable alternatives
- **Honest** - Acknowledge drawbacks alongside benefits

**Specific patterns to follow:**

```markdown
## Context
[2-3 paragraphs setting up the problem]
[Include code examples or protocol specs]
[Explain architectural constraints]

## Decision
[Clear statement: "We will..."]
[1-2 paragraphs explaining the choice]

## Alternatives Considered
[For each alternative:]
  ### Alternative X: [Name]
  [How it would work]
  **Pros:** [Bullet list]
  **Cons:** [Bullet list]

## Rationale
[Why the chosen approach is best]
[Address each key constraint]
[Tie to project goals]

## Consequences
### Positive
[Bullet list of benefits]

### Negative
[Bullet list of costs/drawbacks]

### Neutral
[Bullet list of trade-offs]
```

---

## Context Summary for LLM

**Project**: wlblur - External blur daemon for Wayland  
**Current state**: Milestones 0-2 complete (~4,150 lines)  
**Next phase**: Configuration system (was m-4, now m-3)

**Key architectural constraint**: 
- ext_background_effect_v1 protocol doesn't include blur parameters
- Clients (quickshell) request blur without specifying how
- Compositor must decide parameters when forwarding to daemon
- Preset system minimizes compositor code while enabling future features

**Decision rationale**:
- Enables 220-line compositor integration promise
- Supports multi-compositor consistency
- Enables zero-compositor-change feature upgrades
- Required for material system (m-8)
- Enables hot reload experimentation

**This is not optional** - the architecture requires this approach given the protocol constraints and project goals.

---

## Repository Context Provided

The coding LLM already has this snapshot:
- Milestone m-2 completion confirmed
- Architecture: compositor → wlblurd → libwlblur
- Configuration module currently placeholder
- Public API, IPC protocol, daemon lifecycle documented

**Additional context needed**: The discussion document (from this conversation) explaining why presets are architecturally required.
