# ADR-006: Daemon Configuration with Presets

**Status**: Accepted
**Date**: 2025-11-13

## Context

The wlblur daemon provides blur-as-a-service to Wayland compositors via IPC (ADR-001, ADR-004). A critical architectural question has emerged: **Where should blur configuration parameters live?**

### The Three-Layer Architecture

With wlblur, we have a three-layer stack for blur requests:

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

### The ext_background_effect_v1 Protocol Constraint

The Wayland protocol `ext_background_effect_v1` is the standard way clients request background blur:

```c
// Protocol ONLY allows:
ext_background_effect_surface_v1.set_blur_region(region)

// Does NOT allow:
set_blur_region(region, radius, passes, saturation, ...)
```

**Critical constraint:** The protocol says "blur me" but provides **no blur parameters**. The protocol specification explicitly states:

> "The blur algorithm is subject to compositor policies."

This creates an architectural challenge: When a compositor receives a blur request from a client application, it must decide what blur parameters to send to the wlblur daemon.

### Configuration Architecture Options

**Option 1: IPC-Only Configuration**
- Compositor reads blur config from its own config file
- Compositor sends full parameters (radius, passes, etc.) with every IPC request
- Daemon is stateless, has no configuration

**Option 2: Daemon Configuration with Presets**
- Daemon reads config from `~/.config/wlblur/config.toml`
- Daemon defines named presets ("window", "panel", "hud")
- Compositor sends preset name, daemon resolves to parameters

**Option 3: Hybrid Approach**
- Daemon has optional config with defaults
- Compositor can override with full parameters
- Fallback hierarchy: compositor params → daemon defaults → hardcoded

### Problem Analysis

If we choose IPC-Only Configuration (Option 1):

**Impact on compositor integration:**
```toml
# Compositor config must include ALL blur details
[blur]
radius = 5.0
passes = 3
saturation = 1.1
brightness = 1.0
contrast = 1.0
noise = 0.02
vibrancy = 0.0
```

**When wlblur adds tint support (m-6):**
```toml
# Compositor config must be updated
[blur]
# ... existing fields ...
tint_color = [242, 242, 247, 230]  # NEW - requires compositor config update
tint_strength = 0.9                # NEW - requires compositor config update
```

**When wlblur adds material system (m-8):**
```toml
# Compositor config must be updated again
[blur]
material = "sidebar"  # NEW - requires compositor config update
```

This violates our **minimal compositor integration** goal (~220 lines of code) and creates **maintenance burden** every time wlblur adds features.

### Multi-Compositor Use Case

Consider a user who switches between multiple compositors:

**With IPC-Only Configuration:**
```bash
# Must configure blur separately in each compositor
~/.config/scroll/config.toml  # radius = 5.0, passes = 3
~/.config/niri/config.kdl     # radius = 5.0, passes = 3
~/.config/sway/config         # blur_radius 5.0, blur_passes 3
```

**With Daemon Configuration:**
```bash
# Configure once, works everywhere
~/.config/wlblur/config.toml  # radius = 5.0, passes = 3
```

### Quickshell/Layershell Use Case

Layershell applications (panels, launchers) request blur via `ext_background_effect_v1`. The compositor must decide blur parameters for these surfaces:

**Current approach (Hyprland):**
```ini
# hyprland.conf
blur {
    size = 8
    passes = 3
}

layerrule = blur, waybar
layerrule = blur:1.5, rofi  # Different strength for different apps
```

Hyprland maintains all blur logic in-compositor. When using wlblur, we want to **externalize this configuration** to minimize compositor code.

## Decision

**We will implement daemon-side configuration with named presets (Option 2).**

wlblur will read configuration from `~/.config/wlblur/config.toml` containing:
- Default blur parameters
- Named presets ("window", "panel", "hud", "tooltip")
- Algorithm selection (kawase, gaussian, box, bokeh)
- Daemon settings (socket path, log level)

Compositors will send **preset names** instead of full parameter sets:

```c
// Compositor integration (minimal)
struct wlblur_request req = {
    .preset_name = "panel",  // Just reference preset by name
    // No need to specify radius, passes, saturation, etc.
};
```

The daemon resolves preset names to full parameter sets using its configuration.

### Configuration File Format

```toml
# ~/.config/wlblur/config.toml

[daemon]
socket_path = "/run/user/1000/wlblur.sock"
log_level = "info"
max_nodes_per_client = 100

[defaults]
# System-wide defaults (used when no preset specified)
algorithm = "kawase"
num_passes = 3
radius = 5.0
brightness = 1.0
contrast = 1.0
saturation = 1.1
noise = 0.02
vibrancy = 0.0

[presets.window]
# Regular application windows
algorithm = "kawase"
num_passes = 3
radius = 8.0
saturation = 1.15

[presets.panel]
# Desktop panels (waybar, quickshell)
algorithm = "kawase"
num_passes = 2
radius = 4.0
brightness = 1.05
saturation = 1.1

[presets.hud]
# Overlay/HUD elements (rofi, launchers)
algorithm = "kawase"
num_passes = 4
radius = 12.0
saturation = 1.2
vibrancy = 0.2

[presets.tooltip]
# Tooltips and small popups
algorithm = "kawase"
num_passes = 1
radius = 2.0
saturation = 1.0
```

### Preset Resolution Algorithm

```c
// wlblurd/src/presets.c

struct wlblur_blur_params* resolve_preset(
    const char *preset_name,
    const struct wlblur_blur_params *override_params
) {
    // 1. Try named preset (from config file)
    if (preset_name && preset_name[0] != '\0') {
        struct preset *p = preset_lookup(preset_name);
        if (p) {
            return &p->params;
        }
        log_warn("Preset '%s' not found, using default", preset_name);
    }

    // 2. Try direct parameter override (compositor-provided)
    if (override_params) {
        return override_params;
    }

    // 3. Fall back to daemon defaults (from config or hardcoded)
    return &default_params;
}
```

**Fallback hierarchy:**
1. Named preset (if found in daemon config)
2. Direct parameters (if compositor provides full parameter set)
3. Daemon defaults (from config file or hardcoded)

### Hot Reload Support

The daemon will support configuration hot-reload via SIGUSR1:

```bash
# Edit configuration
vim ~/.config/wlblur/config.toml

# Reload daemon without restart
killall -USR1 wlblurd

# Changes apply immediately to all compositors
```

**Implementation:**
```c
// wlblurd/src/main.c
static void handle_sigusr1(int sig) {
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

signal(SIGUSR1, handle_sigusr1);
```

## Alternatives Considered

### Alternative 1: IPC-Only Configuration

**Approach:** Compositors send full parameter sets with every request. Daemon has no configuration file.

**Pros:**
- Daemon simplicity (no config parsing code)
- Full compositor control over parameters
- No additional process state

**Cons:**
- **Bloats compositor config** with blur implementation details
- **Future features require compositor updates** (tint in m-6, vibrancy in m-7, materials in m-8)
- **Multi-compositor users must duplicate configs** (~/.config/scroll/, ~/.config/niri/, etc.)
- **Violates minimal integration goal** (compositor must know about all blur parameters)
- **No per-surface presets** without compositor implementing preset logic

**Why Rejected:** Defeats the purpose of external daemon. Moves complexity from daemon to compositor, which is exactly what we're trying to avoid (see ADR-001: maintainer resistance to in-compositor blur).

### Alternative 2: Hybrid Approach

**Approach:** Daemon has optional config with defaults. Compositors can override with full parameters if needed.

**Pros:**
- Flexible: supports both simple and advanced use cases
- Backward compatible (daemon works without config)
- Power users can override daemon defaults per-compositor

**Cons:**
- More complex: dual configuration paths
- Ambiguous precedence rules (which config wins?)
- Testing complexity (must test all combinations)
- Documentation burden (explain two ways to configure)

**Why Deferred:** Adds complexity without clear use case. We can add compositor overrides later if needed. Start simple with daemon-only config.

### Alternative 3: Per-Compositor Configuration

**Approach:** Each compositor has its own blur config, no shared daemon config.

**Pros:**
- Compositors have full control
- No shared state concerns
- Familiar to compositor maintainers

**Cons:**
- **Redundant configuration** for multi-compositor users
- **Inconsistent appearance** when switching compositors
- **Doesn't leverage external daemon benefits**
- **Still requires compositor config updates** when wlblur adds features

**Why Rejected:** Doesn't solve the configuration problem, just moves it to each compositor.

### Alternative 4: Wayland Protocol Extension for Parameters

**Approach:** Extend `ext_background_effect_v1` to include blur parameters in the protocol.

**Pros:**
- Client applications could specify their own blur preferences
- Standardized parameter names

**Cons:**
- **Protocol standardization takes years** (waiting for Wayland protocol merge)
- **Doesn't help with layershell** (panels don't want to hardcode blur params)
- **Still requires compositor to handle parameters** (doesn't simplify integration)
- **Client applications shouldn't control visual aesthetics** (that's compositor's job)

**Why Rejected:** Wrong level of abstraction. Blur appearance is a compositor/user preference, not an application choice.

## Consequences

### Positive

1. **Minimal Compositor Integration** (~220 lines maintained)
   - Compositor just maps surface types to preset names
   - No blur parameter knowledge required
   - Example: `preset = is_panel(surface) ? "panel" : "window";`

2. **Future-Proof for Feature Evolution**
   - **m-6 (Tint):** Add `tint_color` to config, zero compositor changes
   - **m-7 (Vibrancy):** Add `vibrancy_strength` to config, zero compositor changes
   - **m-8 (Materials):** Add `material = "sidebar"` to config, zero compositor changes
   - Daemon version updates don't require compositor recompilation

3. **Multi-Compositor Consistency**
   - Single source of truth: `~/.config/wlblur/config.toml`
   - Panels look identical in scroll, niri, sway
   - User configures once, works everywhere

4. **Hot Reload Experimentation**
   - Edit config, send SIGUSR1, see changes immediately
   - No compositor restart needed
   - Fast iteration on blur aesthetics

5. **Preset System Enables Materials**
   - Foundation for macOS-style material system (m-8)
   - Presets like "sidebar", "menu", "titlebar" map to material definitions
   - Users can create custom materials

6. **Clear Evolution Path**
   - Algorithm field in config from day one (even though only kawase supported in m-3)
   - Adding gaussian/box/bokeh in m-9 doesn't break config format
   - Config schema stability

7. **Beginner-Friendly Defaults**
   - Works out-of-box with hardcoded defaults
   - Advanced users can customize via config
   - Gradual complexity curve

### Negative

1. **Daemon Complexity Increase** (+~900 lines)
   - Config parsing: +300 lines (`config.c`)
   - Preset management: +200 lines (`presets.c`)
   - Hot reload: +100 lines (`reload.c`)
   - Structures: +150 lines (`config.h`)
   - IPC protocol extension: +150 lines (`ipc_protocol.c`)

2. **Configuration Dependency**
   - Must add TOML parsing library (tomlc99 recommended)
   - Config file validation required
   - Must handle parse errors gracefully

3. **Testing Complexity**
   - Must test config parsing
   - Must test preset resolution
   - Must test hot reload
   - Must test fallback hierarchy
   - Estimated: +50 test cases

4. **Documentation Burden**
   - User-facing config guide needed
   - Preset customization documentation
   - Algorithm selection guide
   - Troubleshooting common config errors

5. **State Management**
   - Daemon must maintain loaded config in memory
   - Config file watching or manual reload only (no automatic reload on file change)
   - Must validate config before applying (don't break running daemon)

6. **Preset Naming Standardization**
   - Must define standard preset names: "window", "panel", "hud", "tooltip"
   - Compositors must agree on names for interoperability
   - Custom presets must be documented by users

### Neutral

1. **Migration Path** (if we ever want to change approaches)
   - Can add compositor override parameters later (fallback hierarchy supports it)
   - Can add protocol extension later (orthogonal to config)
   - Config file format versioning supports evolution

2. **Performance Impact**
   - Config parsing: +2-5ms at daemon startup (negligible)
   - Memory overhead: +4-8KB for config/presets (negligible)
   - Preset lookup: <0.001ms per request (hash table)
   - No impact on blur rendering performance

## Rationale

### ext_background_effect_v1 Protocol Requirement

The decision is **architecturally required** given:
1. Protocol doesn't include blur parameters
2. Compositor must decide parameters when forwarding to daemon
3. Compositor-side parameter logic violates minimal integration goal

**Alternative approaches fail:**
- IPC-only → Bloats compositor config, requires updates for every wlblur feature
- No config → Compositor must hardcode parameters (inflexible)
- Per-compositor → Redundant configs, inconsistent appearance

**Preset system solves:**
- Compositor sends preset name (1 string)
- Daemon resolves to full parameter set
- Future parameters added to daemon config only

### Alignment with Project Goals

From ADR-001, our goals are:
1. **Minimal compositor changes** (~220 lines)
2. **Multi-compositor support** (scroll, niri, sway work identically)
3. **Independent versioning** (daemon updates don't break compositors)

**Daemon config achieves all three:**
1. Minimal: Compositor just maps surface type → preset name
2. Multi-compositor: Same daemon config works for all
3. Independent: New parameters in daemon config only

### Precedent from Similar Tools

**kanshi** (output configuration daemon):
```bash
~/.config/kanshi/config
killall -HUP kanshi  # Hot reload
```
- Dedicated config file for cross-compositor tool
- Hot reload expected functionality
- Users understand separate configs

**mako** (notification daemon):
```bash
~/.config/mako/config
makoctl reload
```
- External config is standard for Wayland tools
- Reload command user-friendly

**gammastep** (color temperature):
```bash
~/.config/gammastep/config.ini
gammastep -l 40.7:-74.0  # CLI overrides
```
- Hybrid approach: config file + CLI overrides
- Good precedent for our fallback hierarchy

**Conclusion:** Daemon config is the **standard pattern** for compositor-agnostic Wayland tools.

### Implementation Simplicity

**For compositor developers:**
```c
// Entire blur parameter logic:
const char* get_blur_preset(struct wl_surface *surface) {
    if (is_layershell_panel(surface)) return "panel";
    if (is_popup(surface)) return "tooltip";
    return "window";  // Default
}
```

**vs IPC-only approach:**
```c
// Compositor must know all parameters:
struct wlblur_blur_params get_blur_params(struct wl_surface *surface) {
    struct wlblur_blur_params params = {
        .num_passes = config->blur_passes,
        .radius = config->blur_radius,
        .brightness = config->blur_brightness,
        .contrast = config->blur_contrast,
        .saturation = config->blur_saturation,
        .noise = config->blur_noise,
        .vibrancy = config->blur_vibrancy,
    };
    // Must repeat for each surface type...
    return params;
}
```

**Winner:** Preset approach is **~10 lines vs ~50 lines** per compositor.

## Algorithm Selection Strategy

### Decision Within This Decision

Include `algorithm` field in configuration from m-3 onwards, even though only Kawase is initially supported.

### Rationale

**Problem:** Should we add algorithm field now (m-3) or wait until multiple algorithms are implemented (m-9)?

**Decision:** Add now, validate to "kawase" only.

**Why:**
1. **Avoids config format breaking change** - When gaussian/box/bokeh are added in m-9, existing configs keep working
2. **Documents evolution path** - Users know more algorithms are coming
3. **Enables per-preset algorithm selection** - Different presets can use different algorithms
4. **Simple to implement** - Just validate `== "kawase"` in m-3, remove validation in m-9

**Config schema:**
```toml
[presets.window]
algorithm = "kawase"  # Required field (only "kawase" accepted in m-3)
radius = 8.0
passes = 3
```

**Future (m-9):**
```toml
[presets.window]
algorithm = "kawase"    # Quality + speed

[presets.performance]
algorithm = "box"       # NEW: Fastest blur

[presets.artistic]
algorithm = "bokeh"     # NEW: Artistic effect
```

**Validation approach:**
```c
// m-3 implementation (strict)
if (p->params.algorithm != WLBLUR_ALGO_KAWASE) {
    log_error("Preset '%s': only 'kawase' is currently supported", p->name);
    return false;
}

// m-9 implementation (permissive)
switch (p->params.algorithm) {
    case WLBLUR_ALGO_KAWASE:
    case WLBLUR_ALGO_GAUSSIAN:
    case WLBLUR_ALGO_BOX:
    case WLBLUR_ALGO_BOKEH:
        return validate_algorithm_params(&p->params);
    default:
        log_error("Unknown algorithm: %d", p->params.algorithm);
        return false;
}
```

## Implementation Notes

### Configuration File Format: TOML

**Chosen:** TOML (Tom's Obvious Minimal Language)

**Alternatives considered:**
- JSON: Less human-friendly, no comments
- INI: Limited structure, no typing
- YAML: Complex parsing, whitespace-sensitive
- Custom: No tooling support

**Why TOML:**
- Human-readable with comments
- Typed values (int, float, string)
- Good nesting support
- Common in Wayland ecosystem (niri uses TOML)
- Lightweight library available (tomlc99)

**Library:** tomlc99
- Single-header (~2000 lines)
- MIT license (compatible)
- Well-tested
- Used by other Wayland projects

### Standard Preset Names

**Defined standard presets:**
- `window` - Regular application windows
- `panel` - Desktop panels (waybar, quickshell)
- `hud` - Overlay/HUD elements (rofi, launchers)
- `tooltip` - Tooltips and small popups

**Compositors SHOULD:**
- Use standard names when possible
- Document any custom preset names
- Provide sensible fallbacks

**Users CAN:**
- Create custom presets with any names
- Reference them in compositor config
- Share preset collections

### Hot Reload Mechanism

**Signal-based reload:** SIGUSR1

**Why SIGUSR1:**
- Standard Unix mechanism
- Safe for signal handlers (just set flag)
- Used by other daemons (nginx, etc.)
- Easy for users: `killall -USR1 wlblurd`

**Reload safety:**
```c
void handle_reload(int sig) {
    // Parse new config
    struct config *new_config = config_load();
    if (!new_config) {
        log_error("Config reload failed - keeping old config");
        return;  // Don't break running daemon
    }

    // Validate new config
    if (!config_validate(new_config)) {
        log_error("Config validation failed - keeping old config");
        config_free(new_config);
        return;
    }

    // Atomic swap
    struct config *old = global_config;
    global_config = new_config;
    config_free(old);

    log_info("Configuration reloaded successfully");
}
```

### Fallback Hierarchy

**Resolution order:**
1. **Named preset** (if `preset_name` provided and found)
2. **Direct parameters** (if compositor provides full parameter set)
3. **Daemon defaults** (from config file `[defaults]` section)
4. **Hardcoded defaults** (if no config file exists)

**This enables:**
- Works out-of-box (hardcoded defaults)
- Users can customize (config file)
- Compositors can override (direct params)
- Advanced users get full control

## Milestone Reordering

**Original plan:**
- m-3: ScrollWM Integration
- m-4: Testing & Validation

**New plan:**
- **m-3: Configuration System Implementation** (this ADR)
- **m-4: ScrollWM Integration** (requires m-3)
- **m-5: Testing & Validation** (renumbered from m-4)

**Rationale:** ScrollWM integration requires preset system to be functional. Compositor needs to reference presets by name. Can't integrate without configuration system.

## Related Decisions

- **ADR-001:** Why External Daemon - Established need for daemon, motivates configuration externalization
- **ADR-004:** IPC Protocol Design - Will be extended with preset support in m-3
- **m-1:** Shader extraction - Extracted multiple algorithms (gaussian, box, bokeh), algorithm selection in config enables future use
- **m-9:** Additional Blur Algorithms - Algorithm field in config prepares for this milestone

## References

### Internal Documentation
- `docs/post-milestone2-discussion/new-decision-milestone2.5.md` - Complete configuration architecture analysis
- `docs/post-milestone2-discussion/config-blur-selector.md` - Algorithm selection discussion
- `docs/post-milestone2-discussion/notes-for-later.md` - Initial configuration idea
- `docs/post-milestone2-discussion/milestone2-report.md` - Current project state (m-2 complete)

### External Resources
- [ext_background_effect_v1 protocol spec](https://gitlab.freedesktop.org/wayland/wayland-protocols/-/blob/main/staging/ext-background-effect/ext-background-effect-v1.xml) - Protocol constraint source
- [TOML spec](https://toml.io/) - Configuration format
- [tomlc99](https://github.com/cktan/tomlc99) - Parsing library
- [kanshi](https://sr.ht/~emersion/kanshi/) - Similar daemon config pattern
- [mako](https://github.com/emersion/mako) - Similar hot reload pattern

### Compositor Examples
- Hyprland layerrule documentation - Per-surface blur configuration pattern
- niri configuration - TOML format usage in compositor
- Sway configuration - Simple preset-like rules

## Validation

This decision will be validated by:

1. **m-3 Implementation (Weeks 6-8):** Config parsing, presets, hot reload working
2. **m-4 ScrollWM Integration (Weeks 9-10):** Compositor successfully uses preset system
3. **m-6 Tint Addition:** Add tint_color to config without compositor changes
4. **m-9 Algorithm Addition:** Add gaussian/box/bokeh without breaking existing configs
5. **Community Feedback:** Multi-compositor users confirm consistency benefit

## Community Feedback

We invite feedback on this decision:

- **Compositor maintainers:** Is preset-based configuration acceptable for your integration?
- **Users:** Is `~/.config/wlblur/config.toml` a good location? Should we support XDG_CONFIG_HOME?
- **Multi-compositor users:** Does single config source solve your workflow needs?
- **Power users:** Are presets flexible enough, or do you need per-compositor overrides?

Please open issues at the project repository or discuss in the community forum.
