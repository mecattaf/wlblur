# Configuration System Architecture

**Document Version:** 1.0
**Last Updated:** 2025-11-13
**Status:** Specification
**Related:** ADR-006 (Daemon Configuration with Presets)

---

## Table of Contents

1. [Overview](#overview)
2. [Architecture Rationale](#architecture-rationale)
3. [Configuration File Format](#configuration-file-format)
4. [Preset System](#preset-system)
5. [Hot Reload Mechanism](#hot-reload-mechanism)
6. [IPC Protocol Extension](#ipc-protocol-extension)
7. [Implementation Modules](#implementation-modules)
8. [Algorithm Selection](#algorithm-selection)
9. [Error Handling](#error-handling)
10. [Performance Considerations](#performance-considerations)

---

## Overview

The wlblur configuration system provides daemon-side configuration through TOML files, enabling minimal compositor integration while supporting feature evolution. The core innovation is the **preset system**, where compositors reference named parameter sets instead of providing full blur configuration.

### Key Features

- **Daemon-side configuration**: `~/.config/wlblur/config.toml`
- **Named presets**: Standard presets (window, panel, hud, tooltip) + custom user presets
- **Hot reload**: Configuration changes via SIGUSR1 without daemon restart
- **Algorithm selection**: Prepare for multiple blur algorithms (kawase, gaussian, box, bokeh)
- **Fallback hierarchy**: Preset → direct parameters → daemon defaults → hardcoded

### System Architecture

```
┌──────────────────────────────────────┐
│ User                                 │
│                                      │
│  Edits ~/.config/wlblur/config.toml │
│  Sends SIGUSR1 to wlblurd            │
└──────────────┬───────────────────────┘
               │
               ↓
┌──────────────────────────────────────┐
│ wlblurd Daemon                       │
│                                      │
│  ┌────────────────────────────────┐ │
│  │ Config Parser (config.c)       │ │
│  │  - TOML parsing                │ │
│  │  - Validation                  │ │
│  │  - Defaults                    │ │
│  └───────────┬────────────────────┘ │
│              ↓                       │
│  ┌────────────────────────────────┐ │
│  │ Preset Manager (presets.c)     │ │
│  │  - Preset lookup (hash table)  │ │
│  │  - Fallback resolution         │ │
│  │  - Standard presets            │ │
│  └───────────┬────────────────────┘ │
│              ↓                       │
│  ┌────────────────────────────────┐ │
│  │ Hot Reload (reload.c)          │ │
│  │  - SIGUSR1 handler             │ │
│  │  - Safe config swap            │ │
│  │  - Error recovery              │ │
│  └────────────────────────────────┘ │
└──────────────┬───────────────────────┘
               │ IPC
               ↓
┌──────────────────────────────────────┐
│ Compositor                           │
│                                      │
│  Sends preset_name = "panel"        │
│  Receives full parameters           │
└──────────────────────────────────────┘
```

---

## Architecture Rationale

### Why Daemon Configuration?

The decision for daemon-side configuration (vs compositor-side) is driven by the **ext_background_effect_v1 protocol constraint**:

```c
// Protocol ONLY allows:
ext_background_effect_surface_v1.set_blur_region(region)

// Does NOT include parameters:
// NO: set_blur_region(region, radius, passes, saturation, ...)
```

**The problem:** Client applications (quickshell, waybar) request blur without specifying parameters. The compositor must decide parameters when forwarding to wlblur daemon.

**Three options considered:**

1. **Compositor stores parameters** → Bloats compositor config, requires updates for every wlblur feature
2. **Daemon stores parameters** → Minimal compositor code, future-proof
3. **Hybrid** → Too complex for initial implementation

**Decision:** Daemon stores parameters (option 2). See ADR-006 for complete analysis.

### Benefits

1. **Minimal Compositor Integration** (~220 lines)
   - Compositor maps surface type → preset name
   - No blur parameter knowledge required

2. **Future-Proof**
   - m-6 adds tint: no compositor changes
   - m-7 adds vibrancy: no compositor changes
   - m-8 adds materials: no compositor changes

3. **Multi-Compositor Consistency**
   - Single config: `~/.config/wlblur/config.toml`
   - Panels look identical in scroll, niri, sway

4. **Hot Reload Experimentation**
   - Edit config, send SIGUSR1, see changes
   - No compositor restart needed

---

## Configuration File Format

### File Location

**Priority order:**

1. `$XDG_CONFIG_HOME/wlblur/config.toml`
2. `~/.config/wlblur/config.toml`
3. `/etc/wlblur/config.toml` (system-wide)
4. Hardcoded defaults (no file)

### TOML Format

**Why TOML?**

- Human-readable with comments
- Typed values (int, float, string)
- Good nesting support
- Common in Wayland ecosystem (niri, kanshi)
- Lightweight library (tomlc99)

**Alternatives rejected:**

- JSON: No comments, less human-friendly
- INI: Limited structure, no typing
- YAML: Complex parsing, whitespace-sensitive
- Custom: No tooling support

### Complete Schema

```toml
# ~/.config/wlblur/config.toml

[daemon]
# Daemon settings
socket_path = "/run/user/1000/wlblur.sock"  # Unix socket path
log_level = "info"                          # debug, info, warn, error
max_nodes_per_client = 100                  # Resource limit

[defaults]
# System-wide default parameters (used when no preset specified)
algorithm = "kawase"       # Blur algorithm: kawase (only option in m-3)
num_passes = 3             # 1-8, number of blur iterations
radius = 5.0               # 1.0-20.0, blur strength
brightness = 1.0           # 0.0-2.0, brightness adjustment
contrast = 1.0             # 0.0-2.0, contrast adjustment
saturation = 1.1           # 0.0-2.0, saturation adjustment
noise = 0.02               # 0.0-1.0, noise overlay strength
vibrancy = 0.0             # 0.0-2.0, HSL saturation boost (Hyprland-style)

[presets.window]
# Regular application windows
algorithm = "kawase"
num_passes = 3
radius = 8.0
saturation = 1.15
brightness = 1.0
contrast = 1.0
noise = 0.02
vibrancy = 0.0

[presets.panel]
# Desktop panels (waybar, quickshell, etc.)
algorithm = "kawase"
num_passes = 2
radius = 4.0
brightness = 1.05
saturation = 1.1
noise = 0.01
vibrancy = 0.0

[presets.hud]
# Overlay/HUD elements (rofi, launchers, notifications)
algorithm = "kawase"
num_passes = 4
radius = 12.0
saturation = 1.2
brightness = 1.0
contrast = 1.0
noise = 0.02
vibrancy = 0.2

[presets.tooltip]
# Tooltips and small popups
algorithm = "kawase"
num_passes = 1
radius = 2.0
saturation = 1.0
brightness = 1.0
contrast = 1.0
noise = 0.0
vibrancy = 0.0

[presets.custom_panel]
# Example user-defined preset
algorithm = "kawase"
num_passes = 2
radius = 5.5
brightness = 1.08
saturation = 1.15
noise = 0.015
vibrancy = 0.05

# Future (m-9): Algorithm-specific parameters
# [presets.artistic]
# algorithm = "bokeh"
# radius = 12.0
# bokeh_rotation = 45.0
# bokeh_sides = 6         # Hexagonal bokeh
# bokeh_roundness = 0.5
# saturation = 1.3
# vibrancy = 0.2
```

### Validation Rules

**Parameter ranges:**

| Parameter | Type | Range | Default |
|-----------|------|-------|---------|
| `algorithm` | string | "kawase" (m-3 only) | "kawase" |
| `num_passes` | int | 1-8 | 3 |
| `radius` | float | 1.0-20.0 | 5.0 |
| `brightness` | float | 0.0-2.0 | 1.0 |
| `contrast` | float | 0.0-2.0 | 1.0 |
| `saturation` | float | 0.0-2.0 | 1.1 |
| `noise` | float | 0.0-1.0 | 0.02 |
| `vibrancy` | float | 0.0-2.0 | 0.0 |

**Validation logic:**

```c
// wlblurd/src/config.c

bool validate_blur_params(const struct wlblur_blur_params *params) {
    // Algorithm (m-3: only kawase)
    if (params->algorithm != WLBLUR_ALGO_KAWASE) {
        log_error("Only 'kawase' algorithm supported in this version");
        return false;
    }

    // Range checks
    if (params->num_passes < 1 || params->num_passes > 8) {
        log_error("num_passes must be 1-8, got %d", params->num_passes);
        return false;
    }

    if (params->radius < 1.0 || params->radius > 20.0) {
        log_error("radius must be 1.0-20.0, got %.1f", params->radius);
        return false;
    }

    // ... more range checks ...

    return true;
}
```

---

## Preset System

### Standard Presets

**Four standard presets** defined by wlblur project:

1. **`window`** - Regular application windows
   - Use case: Normal desktop applications
   - Parameters: Moderate blur (radius=8.0, passes=3)

2. **`panel`** - Desktop panels
   - Use case: waybar, quickshell, system panels
   - Parameters: Light blur (radius=4.0, passes=2)

3. **`hud`** - Overlay/HUD elements
   - Use case: rofi, launchers, notifications
   - Parameters: Strong blur (radius=12.0, passes=4, vibrancy=0.2)

4. **`tooltip`** - Tooltips and small popups
   - Use case: Tooltips, context menus
   - Parameters: Minimal blur (radius=2.0, passes=1)

### Preset Naming Convention

**Guidelines:**

- Use lowercase
- Separate words with underscores
- Descriptive names (e.g., `panel_bright`, `window_subtle`)
- Avoid compositor-specific names

**Examples:**

- Good: `panel`, `hud`, `window_minimal`, `notification_strong`
- Bad: `MyBlur`, `hyprland-style`, `blur1`

### Preset Lookup

**Data structure (hash table for O(1) lookup):**

```c
// wlblurd/src/presets.c

struct preset {
    char name[32];                    // Preset name
    struct wlblur_blur_params params; // Full parameter set
    struct preset *next;              // Hash table chaining
};

struct preset_registry {
    struct preset *buckets[64];       // Hash table (64 buckets)
    size_t preset_count;
};

// Hash function
static uint32_t hash_preset_name(const char *name) {
    uint32_t hash = 5381;
    for (const char *p = name; *p; p++) {
        hash = ((hash << 5) + hash) + *p;
    }
    return hash % 64;
}

// Lookup
struct preset* preset_lookup(const char *name) {
    uint32_t bucket = hash_preset_name(name);
    for (struct preset *p = registry.buckets[bucket]; p; p = p->next) {
        if (strcmp(p->name, name) == 0) {
            return p;
        }
    }
    return NULL;  // Not found
}
```

### Preset Resolution Algorithm

**Fallback hierarchy:**

```
1. Named preset (from config file)
   ↓ (if not found)
2. Direct parameters (from compositor)
   ↓ (if not provided)
3. Daemon defaults (from config [defaults])
   ↓ (if config missing)
4. Hardcoded defaults (builtin)
```

**Implementation:**

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
            log_debug("Using preset '%s'", preset_name);
            return &p->params;
        }
        log_warn("Preset '%s' not found, falling back", preset_name);
    }

    // 2. Try direct parameter override
    if (override_params) {
        log_debug("Using compositor-provided parameters");
        return override_params;
    }

    // 3. Try daemon defaults (from config)
    if (global_config && global_config->has_defaults) {
        log_debug("Using daemon defaults");
        return &global_config->defaults;
    }

    // 4. Fall back to hardcoded defaults
    log_debug("Using hardcoded defaults");
    static struct wlblur_blur_params hardcoded = {
        .algorithm = WLBLUR_ALGO_KAWASE,
        .num_passes = 3,
        .radius = 5.0,
        .brightness = 1.0,
        .contrast = 1.0,
        .saturation = 1.1,
        .noise = 0.02,
        .vibrancy = 0.0,
    };
    return &hardcoded;
}
```

---

## Hot Reload Mechanism

### Signal-Based Reload

**Why SIGUSR1?**

- Standard Unix mechanism for configuration reload
- Used by nginx, kanshi, mako
- Safe for signal handlers (just set flag)
- Easy for users: `killall -USR1 wlblurd`

**User workflow:**

```bash
# 1. Edit configuration
vim ~/.config/wlblur/config.toml

# 2. Reload daemon
killall -USR1 wlblurd

# 3. Changes apply immediately (no compositor restart)
```

### Implementation

**Signal handler (sets flag only):**

```c
// wlblurd/src/main.c

static volatile sig_atomic_t reload_requested = 0;

static void signal_handler(int sig) {
    if (sig == SIGUSR1) {
        reload_requested = 1;
    }
}

int main(int argc, char *argv[]) {
    // Setup signal handler
    struct sigaction sa = {
        .sa_handler = signal_handler,
        .sa_flags = SA_RESTART,
    };
    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);

    // Main event loop
    while (running) {
        // Check reload flag
        if (reload_requested) {
            reload_requested = 0;
            handle_config_reload();
        }

        // Handle IPC events...
    }
}
```

**Reload function (safe, atomic):**

```c
// wlblurd/src/reload.c

void handle_config_reload(void) {
    log_info("Reloading configuration...");

    // 1. Parse new config
    struct daemon_config *new_config = config_load(NULL);
    if (!new_config) {
        log_error("Failed to load config - keeping old configuration");
        return;
    }

    // 2. Validate new config
    if (!config_validate(new_config)) {
        log_error("Config validation failed - keeping old configuration");
        config_free(new_config);
        return;
    }

    // 3. Atomic swap (safe even during IPC requests)
    struct daemon_config *old_config = global_config;
    global_config = new_config;

    // 4. Free old config
    if (old_config) {
        config_free(old_config);
    }

    log_info("Configuration reloaded successfully");
    log_info("  Presets loaded: %zu", new_config->preset_count);
}
```

### Reload Safety

**Guarantees:**

1. **Never crash daemon** - Parse errors keep old config
2. **Atomic swap** - No half-loaded state visible to IPC requests
3. **Validation before apply** - Invalid configs rejected
4. **Logging** - Clear feedback about success/failure

**Error handling:**

```c
// Reload error scenarios

// 1. File not found
if (errno == ENOENT) {
    log_error("Config file not found, keeping old config");
    // Daemon continues with old config or hardcoded defaults
}

// 2. Parse error
if (!toml_parse(config_path)) {
    log_error("TOML parse error at line %d: %s", line, error);
    log_error("Keeping old configuration");
    // Old config preserved
}

// 3. Validation error
if (params->radius > 20.0) {
    log_error("Preset '%s': radius %.1f exceeds maximum 20.0", name, radius);
    log_error("Keeping old configuration");
    // Old config preserved
}
```

---

## IPC Protocol Extension

### Extended Request Structure

**Before (m-2):**

```c
struct wlblur_request {
    uint32_t protocol_version;
    uint32_t op;
    uint32_t node_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;
    struct wlblur_blur_params params;  // Always provided
};
```

**After (m-3):**

```c
struct wlblur_request {
    uint32_t protocol_version;
    uint32_t op;
    uint32_t node_id;
    uint32_t width;
    uint32_t height;
    uint32_t format;

    // NEW: Preset support
    char preset_name[32];              // Preset name (or empty string)
    bool use_preset;                   // true = use preset, false = use params

    struct wlblur_blur_params params;  // Optional override
};
```

**Backward compatibility:**

```c
// Old clients (set use_preset = false)
struct wlblur_request req = {
    .use_preset = false,
    .params = {.radius = 5.0, .passes = 3, ...},
};

// New clients (use preset)
struct wlblur_request req = {
    .use_preset = true,
    .preset_name = "panel",
    // params ignored when use_preset = true
};
```

### IPC Handler Integration

**Preset resolution in IPC handler:**

```c
// wlblurd/src/ipc_protocol.c

void handle_render_request(int client_fd, struct wlblur_request *req) {
    // Resolve preset to parameters
    struct wlblur_blur_params *params;

    if (req->use_preset) {
        // Preset mode: resolve name
        params = resolve_preset(req->preset_name, NULL);
        if (!params) {
            send_error(client_fd, WLBLUR_ERROR_INVALID_PRESET);
            return;
        }
        log_debug("Client %d using preset '%s'", client_fd, req->preset_name);
    } else {
        // Direct parameter mode: validate
        if (!validate_blur_params(&req->params)) {
            send_error(client_fd, WLBLUR_ERROR_INVALID_PARAMS);
            return;
        }
        params = &req->params;
        log_debug("Client %d using direct parameters", client_fd);
    }

    // Apply blur with resolved parameters
    int result_fd = apply_blur(req->input_fd, params);
    send_response(client_fd, result_fd);
}
```

---

## Implementation Modules

### Module Overview

```
wlblurd/src/
├── config.c          (~300 lines) - TOML parsing, validation
├── presets.c         (~200 lines) - Preset management, lookup
├── reload.c          (~100 lines) - Hot reload logic
├── main.c            (+50 lines)  - Signal handler setup
└── ipc_protocol.c    (+100 lines) - Preset resolution in IPC

wlblurd/include/
├── config.h          (~150 lines) - Config structures, API
└── protocol.h        (+20 lines)  - Extended IPC protocol

libwlblur/include/wlblur/
└── blur_params.h     (+30 lines)  - Algorithm enum
```

### config.c - Configuration Parser

**Responsibilities:**

- Load TOML file from disk
- Parse daemon settings, defaults, presets
- Validate all parameters
- Handle missing files gracefully

**API:**

```c
// wlblurd/include/config.h

struct daemon_config {
    // Daemon settings
    char socket_path[256];
    char log_level[16];
    uint32_t max_nodes_per_client;

    // Default parameters
    bool has_defaults;
    struct wlblur_blur_params defaults;

    // Presets
    struct preset_registry presets;
    size_t preset_count;
};

// Load config from file (NULL = default path)
struct daemon_config* config_load(const char *path);

// Validate config
bool config_validate(const struct daemon_config *config);

// Free config
void config_free(struct daemon_config *config);
```

### presets.c - Preset Manager

**Responsibilities:**

- Maintain preset registry (hash table)
- Preset lookup by name
- Fallback resolution algorithm
- Hardcoded default presets

**API:**

```c
// wlblurd/include/config.h

// Initialize preset registry with hardcoded defaults
void preset_registry_init(struct preset_registry *registry);

// Add preset from config
bool preset_registry_add(struct preset_registry *registry,
                         const char *name,
                         const struct wlblur_blur_params *params);

// Lookup preset by name
struct preset* preset_lookup(const struct preset_registry *registry,
                             const char *name);

// Resolve preset with fallback
struct wlblur_blur_params* resolve_preset(
    const char *preset_name,
    const struct wlblur_blur_params *override_params);
```

### reload.c - Hot Reload

**Responsibilities:**

- SIGUSR1 signal handling
- Safe config reload
- Atomic config swap
- Error recovery

**API:**

```c
// wlblurd/include/config.h

// Setup signal handlers
void reload_init(void);

// Handle config reload (called from main loop)
void handle_config_reload(void);

// Check if reload requested
bool reload_pending(void);
```

---

## Algorithm Selection

### Algorithm Enum

**Added to parameter structure:**

```c
// libwlblur/include/wlblur/blur_params.h

enum wlblur_algorithm {
    WLBLUR_ALGO_KAWASE = 0,      // Dual Kawase (m-3 default)
    WLBLUR_ALGO_GAUSSIAN = 1,    // Gaussian (m-9)
    WLBLUR_ALGO_BOX = 2,         // Box blur (m-9)
    WLBLUR_ALGO_BOKEH = 3,       // Bokeh (m-9)
};

struct wlblur_blur_params {
    enum wlblur_algorithm algorithm;  // NEW in m-3

    // Existing parameters
    int num_passes;
    float radius;
    float brightness;
    float contrast;
    float saturation;
    float noise;
    float vibrancy;
};
```

### Why Include Algorithm Field in m-3?

**Rationale:**

1. **Avoid breaking change** - When adding gaussian/box/bokeh in m-9, existing configs keep working
2. **Document evolution path** - Users know more algorithms are coming
3. **Simple validation** - Just check `== WLBLUR_ALGO_KAWASE` in m-3

**m-3 validation (strict):**

```c
// wlblurd/src/config.c (m-3 implementation)

bool validate_algorithm(enum wlblur_algorithm algo) {
    if (algo != WLBLUR_ALGO_KAWASE) {
        log_error("Only 'kawase' algorithm supported in this version");
        log_error("Gaussian, box, and bokeh will be added in v2.0");
        return false;
    }
    return true;
}
```

**m-9 validation (permissive):**

```c
// wlblurd/src/config.c (m-9 implementation)

bool validate_algorithm(enum wlblur_algorithm algo) {
    switch (algo) {
        case WLBLUR_ALGO_KAWASE:
        case WLBLUR_ALGO_GAUSSIAN:
        case WLBLUR_ALGO_BOX:
        case WLBLUR_ALGO_BOKEH:
            return true;
        default:
            log_error("Unknown algorithm: %d", algo);
            return false;
    }
}
```

---

## Error Handling

### Configuration Parse Errors

**Error types:**

1. **File not found** - Use hardcoded defaults
2. **TOML syntax error** - Log error, reject config
3. **Missing required field** - Log warning, use default value
4. **Invalid value** - Log error, reject preset/config
5. **Out of range** - Log error, reject preset/config

**Error reporting:**

```c
// wlblurd/src/config.c

enum config_error {
    CONFIG_OK = 0,
    CONFIG_FILE_NOT_FOUND,
    CONFIG_PARSE_ERROR,
    CONFIG_VALIDATION_ERROR,
};

struct config_error_info {
    enum config_error code;
    int line;                 // Line number (for parse errors)
    char message[256];        // Human-readable error
};

struct daemon_config* config_load_with_error(
    const char *path,
    struct config_error_info *error_out
);
```

### Runtime Errors

**Preset not found:**

```c
// Compositor requests preset "custom_foo" that doesn't exist

if (!preset_lookup("custom_foo")) {
    log_warn("Preset 'custom_foo' not found, using defaults");
    // Fall back to defaults, don't fail request
}
```

**Invalid algorithm:**

```c
// User specifies algorithm = "gaussian" in m-3

if (algorithm != "kawase") {
    log_error("Preset '%s': algorithm '%s' not yet supported", name, algorithm);
    log_error("Currently only 'kawase' is available");
    // Reject entire preset during config load
}
```

---

## Performance Considerations

### Config Loading Performance

**Target:** <5ms startup overhead

**Measurements:**

| Operation | Time | Notes |
|-----------|------|-------|
| TOML parse | ~2ms | File I/O + parsing |
| Preset initialization | ~0.5ms | Hash table setup |
| Validation | ~0.5ms | Range checks |
| **Total** | **~3ms** | ✅ Within budget |

**Optimization:**

- Lazy preset loading (parse on first use)
- Preset caching (keep parsed config in memory)
- Fast hash function for preset lookup

### Preset Lookup Performance

**Target:** <0.001ms per lookup (negligible overhead)

**Implementation:**

```c
// Hash table: O(1) average case
uint32_t hash = hash_preset_name("panel");  // ~10ns
struct preset *p = buckets[hash];           // ~5ns
while (p && strcmp(p->name, "panel") != 0) { p = p->next; }  // ~20ns
// Total: ~35ns = 0.000035ms ✅
```

**Load factor:** Keep buckets <75% full (minimize collisions)

### Hot Reload Performance

**Target:** <10ms reload time (user doesn't notice)

**Measurements:**

| Operation | Time | Notes |
|-----------|------|-------|
| TOML parse | ~2ms | Reload file from disk |
| Validation | ~0.5ms | Check all presets |
| Atomic swap | <0.001ms | Pointer update |
| Free old config | ~0.5ms | Memory deallocation |
| **Total** | **~3ms** | ✅ Imperceptible |

**Reload doesn't block IPC:** Reload runs in main thread, but IPC processing can continue using old config until swap completes.

---

## Future Extensions

### m-9: Multiple Algorithms

**Config with algorithm selection:**

```toml
[presets.window_gaussian]
algorithm = "gaussian"
radius = 8.0
gaussian_sigma = 10.0
gaussian_kernel_size = 21

[presets.window_fast]
algorithm = "box"
radius = 6.0
box_iterations = 2

[presets.artistic]
algorithm = "bokeh"
radius = 12.0
bokeh_rotation = 45.0
bokeh_sides = 6
```

**Parameter structure extension:**

```c
struct wlblur_blur_params {
    enum wlblur_algorithm algorithm;

    // Universal parameters
    float radius;

    // Algorithm-specific parameters (union)
    union {
        struct {
            int num_passes;
            float offset;
        } kawase;

        struct {
            float sigma;
            int kernel_size;
        } gaussian;

        struct {
            int iterations;
        } box;

        struct {
            float rotation;
            int sides;
            float roundness;
        } bokeh;
    } algo_params;

    // Post-processing (universal)
    float brightness;
    float contrast;
    float saturation;
    float noise;
    float vibrancy;
};
```

### m-8: Material System

**Config with materials:**

```toml
[materials.sidebar]
# Material = preset + appearance mode + dynamic adaptation
base_preset = "panel"
appearance_mode = "auto"  # light, dark, auto
adapt_to_wallpaper = true
adapt_to_content = true

[materials.hud]
base_preset = "hud"
appearance_mode = "dark"
adapt_to_wallpaper = false
```

---

## References

### Internal Documentation

- **ADR-006:** Daemon Configuration with Presets - Architecture decision
- **milestone m-3:** Configuration System Implementation - Deliverables
- **docs/post-milestone2-discussion/new-decision-milestone2.5.md** - Configuration discussion

### External Resources

- **TOML spec:** https://toml.io/ - Configuration format specification
- **tomlc99:** https://github.com/cktan/tomlc99 - Parsing library
- **kanshi:** https://sr.ht/~emersion/kanshi/ - Similar config pattern (Wayland output manager)
- **mako:** https://github.com/emersion/mako - Hot reload pattern (Wayland notification daemon)

### Related Architecture

- **01-libwlblur.md:** Library internals (uses blur_params)
- **02-wlblurd.md:** Daemon architecture (integrates config system)
- **03-integration.md:** Compositor integration (preset mapping)

---

**Document End**

For implementation details, see:
- `wlblurd/src/config.c` - Configuration parser
- `wlblurd/src/presets.c` - Preset manager
- `wlblurd/src/reload.c` - Hot reload implementation
