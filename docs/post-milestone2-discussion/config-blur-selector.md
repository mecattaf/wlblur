# Algorithm Selection Must Be Addressed

This is a critical design decision for the configuration architecture that impacts:

1. **Config schema stability** (add now vs breaking change later)
2. **Preset flexibility** (different algorithms per preset)
3. **Parameter structure** (algorithm-specific params)

Let me add this to the handoff document.

---

## Algorithm Selection in Configuration

### Context Reminder

**From milestone m-1 (shader extraction):**
- ✅ Dual Kawase extracted and implemented
- ✅ Gaussian shader extracted (Wayfire)
- ✅ Box blur shader extracted (Wayfire)
- ✅ Bokeh blur shader extracted (Wayfire)

**Current state:**
- Only Dual Kawase is implemented in libwlblur
- Other algorithms deferred to m-9 (weeks 22-24)

**The question:** Should TOML config include algorithm selection now, or wait until m-9?

---

## Recommendation: Include Algorithm Field Now

### Proposed Config Schema

```toml
# ~/.config/wlblur/config.toml

[defaults]
algorithm = "kawase"  # Currently only supported option
                      # Future: "gaussian", "box", "bokeh"
radius = 5.0
passes = 3

[presets.window]
algorithm = "kawase"
radius = 8.0
passes = 3
saturation = 1.15

[presets.panel]
algorithm = "kawase"
radius = 4.0
passes = 2

# Future possibility (documented but not yet implemented):
[presets.artistic]
algorithm = "bokeh"   # Will be supported in m-9
radius = 12.0
rotation = 45.0       # Bokeh-specific parameter
shape = "hexagon"     # Bokeh-specific parameter

[presets.performance]
algorithm = "box"     # Will be supported in m-9 (fastest)
radius = 4.0
passes = 2
```

### Rationale

**1. Config Format Stability**
- Avoid breaking changes in m-9
- Users won't need to rewrite configs
- Preset definitions remain compatible

**2. Clear Evolution Path**
- Users know more algorithms are coming
- Documentation can explain future options
- Preset system is future-proof from day one

**3. Per-Preset Algorithm Selection**
```toml
[presets.window]
algorithm = "kawase"    # Quality + performance

[presets.artistic]
algorithm = "bokeh"     # Artistic effect (future)

[presets.performance]
algorithm = "box"       # Maximum speed (future)
```

**4. Implementation Simplicity Now**
```c
// wlblurd/src/presets.c (m-3 implementation)

struct wlblur_blur_params* load_preset(const char *name) {
    // Parse TOML
    const char *algo = toml_get_string(preset, "algorithm");
    
    // Validate (only kawase supported in m-3)
    if (strcmp(algo, "kawase") != 0) {
        log_warn("Algorithm '%s' not yet implemented, using kawase", algo);
        algo = "kawase";
    }
    
    // Load parameters...
}
```

**5. Easy Extension in m-9**
```c
// m-9: Just remove validation, add algorithm dispatch
if (strcmp(algo, "gaussian") == 0) {
    return apply_gaussian_blur(&params);
} else if (strcmp(algo, "bokeh") == 0) {
    return apply_bokeh_blur(&params);
}
// No config format change needed!
```

---

## Parameter Structure Implications

### Current Parameter Structure (Kawase-centric)

```c
// libwlblur/include/wlblur/blur_params.h

struct wlblur_blur_params {
    int num_passes;      // Kawase/Box: number of iterations
    float radius;        // Universal: blur strength
    float brightness;    // Post-processing
    float contrast;      // Post-processing
    float saturation;    // Post-processing
    float noise;         // Post-processing
    float vibrancy;      // Post-processing (Hyprland-style)
};
```

### Extended Parameter Structure (Multi-algorithm)

```c
// libwlblur/include/wlblur/blur_params.h

enum wlblur_algorithm {
    WLBLUR_ALGO_KAWASE = 0,      // Default
    WLBLUR_ALGO_GAUSSIAN = 1,    // m-9
    WLBLUR_ALGO_BOX = 2,         // m-9
    WLBLUR_ALGO_BOKEH = 3,       // m-9
};

struct wlblur_blur_params {
    // Algorithm selection (NEW in m-3)
    enum wlblur_algorithm algorithm;
    
    // Universal parameters (work for all algorithms)
    float radius;           // Blur strength
    
    // Algorithm-specific parameters
    union {
        struct {
            int num_passes;      // Kawase: 1-8
            float offset;        // Kawase: sample offset
        } kawase;
        
        struct {
            float sigma;         // Gaussian: standard deviation
            int kernel_size;     // Gaussian: kernel dimensions
        } gaussian;
        
        struct {
            int iterations;      // Box: number of passes
        } box;
        
        struct {
            float rotation;      // Bokeh: rotation angle
            int sides;           // Bokeh: 5=pentagon, 6=hexagon
            float roundness;     // Bokeh: corner rounding
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

**Alternative: Keep simple for m-3, extend in m-9:**

```c
// m-3 version (backward compatible)
struct wlblur_blur_params {
    enum wlblur_algorithm algorithm;  // NEW
    
    // Keep existing fields for now
    int num_passes;      // Kawase (ignored by other algorithms)
    float radius;        // Universal
    
    // Post-processing
    float brightness;
    float contrast;
    float saturation;
    float noise;
    float vibrancy;
    
    // Reserved for future algorithm-specific params
    void *algo_specific;  // NULL in m-3, populated in m-9
};
```

---

## TOML Configuration Examples

### Basic Configuration (m-3)

```toml
# ~/.config/wlblur/config.toml

[defaults]
algorithm = "kawase"
radius = 5.0
passes = 3
saturation = 1.1

[presets.window]
algorithm = "kawase"
radius = 8.0
passes = 3

[presets.panel]
algorithm = "kawase"
radius = 4.0
passes = 2
```

### Advanced Configuration (m-9 and beyond)

```toml
# ~/.config/wlblur/config.toml

[defaults]
algorithm = "kawase"
radius = 5.0

# Standard window blur
[presets.window]
algorithm = "kawase"
radius = 8.0
kawase_passes = 3
kawase_offset = 1.0

# High-quality Gaussian for important surfaces
[presets.window_gaussian]
algorithm = "gaussian"
radius = 8.0
gaussian_sigma = 10.0
gaussian_kernel_size = 21

# Fast box blur for performance
[presets.panel_fast]
algorithm = "box"
radius = 4.0
box_iterations = 2

# Artistic bokeh effect for special elements
[presets.hud_artistic]
algorithm = "bokeh"
radius = 12.0
bokeh_rotation = 45.0
bokeh_sides = 6        # Hexagonal bokeh
bokeh_roundness = 0.5
saturation = 1.3
vibrancy = 0.2

# Algorithm comparison preset
[presets.comparison]
algorithm = "kawase"  # Can be changed for A/B testing
radius = 8.0
```

---

## Validation Strategy

### m-3 Implementation (Strict)

```c
// wlblurd/src/config.c

bool validate_preset(struct preset *p) {
    // Only kawase supported in m-3
    if (p->params.algorithm != WLBLUR_ALGO_KAWASE) {
        log_error("Preset '%s': algorithm '%s' not yet supported",
                  p->name, algorithm_name(p->params.algorithm));
        log_error("Currently only 'kawase' is available");
        return false;
    }
    
    // Validate kawase-specific parameters
    if (p->params.num_passes < 1 || p->params.num_passes > 8) {
        log_error("Preset '%s': passes must be 1-8", p->name);
        return false;
    }
    
    return true;
}
```

### m-9 Implementation (Permissive)

```c
// wlblurd/src/config.c (updated)

bool validate_preset(struct preset *p) {
    // All algorithms now supported
    switch (p->params.algorithm) {
    case WLBLUR_ALGO_KAWASE:
        return validate_kawase_params(&p->params);
    case WLBLUR_ALGO_GAUSSIAN:
        return validate_gaussian_params(&p->params);
    case WLBLUR_ALGO_BOX:
        return validate_box_params(&p->params);
    case WLBLUR_ALGO_BOKEH:
        return validate_bokeh_params(&p->params);
    default:
        log_error("Unknown algorithm: %d", p->params.algorithm);
        return false;
    }
}
```

---

## Documentation Updates Needed

### 1. ADR 006 Must Address Algorithm Selection

Add section:

```markdown
## Algorithm Selection Strategy

### Decision
Include `algorithm` field in configuration from m-3 onwards,
even though only Kawase is initially supported.

### Rationale
- Avoids config format breaking change in m-9
- Provides clear evolution path to users
- Enables per-preset algorithm selection
- Documents future capabilities

### m-3 Implementation
- Only `algorithm = "kawase"` is accepted
- Other values logged as warnings, default to kawase
- Config schema is stable for future

### m-9 Extension
- Add gaussian, box, bokeh support
- No config format change required
- Users can opt-in to new algorithms per preset
```

### 2. Configuration Architecture Doc

**File**: `docs/architecture/04-configuration-system.md`

Add section:

```markdown
## Algorithm Selection

### Supported Algorithms

**m-3 (Current)**
- `kawase` - Dual Kawase blur (default)

**m-9 (Planned)**
- `gaussian` - High-quality Gaussian blur
- `box` - Fast box blur for performance
- `bokeh` - Artistic depth-of-field effect

### Configuration

```toml
[presets.window]
algorithm = "kawase"  # Required field
radius = 8.0
passes = 3
```

### Per-Preset Selection

Different presets can use different algorithms:

```toml
[presets.window]
algorithm = "kawase"    # Quality + speed

[presets.performance]
algorithm = "box"       # Maximum speed (m-9)

[presets.artistic]
algorithm = "bokeh"     # Artistic effect (m-9)
```

### Validation

m-3: Only "kawase" accepted, others rejected with error
m-9: All algorithms supported, algorithm-specific validation
```

### 3. User Configuration Guide

**File**: `docs/configuration-guide.md`

Add section:

```markdown
## Choosing a Blur Algorithm

### Available Algorithms

**Dual Kawase** (default)
- Best balance of quality and performance
- Multiple passes for smooth blur
- ~1.2ms @ 1920×1080

```toml
[presets.window]
algorithm = "kawase"
radius = 8.0
passes = 3
```

**Gaussian** (coming in v2.0)
- Highest quality blur
- Perfect mathematical distribution
- ~1.8ms @ 1920×1080

```toml
[presets.hq]
algorithm = "gaussian"
radius = 10.0
sigma = 8.0
```

**Box** (coming in v2.0)
- Fastest algorithm
- Lower quality but great performance
- ~0.6ms @ 1920×1080

```toml
[presets.fast]
algorithm = "box"
radius = 6.0
iterations = 2
```

**Bokeh** (coming in v2.0)
- Artistic depth-of-field effect
- Camera lens-like blur
- ~2.5ms @ 1920×1080

```toml
[presets.artistic]
algorithm = "bokeh"
radius = 12.0
rotation = 45.0
sides = 6
```

### When to Use Each Algorithm

- **kawase**: Default choice, works for everything
- **gaussian**: When quality is paramount
- **box**: When performance is critical
- **bokeh**: For artistic/decorative surfaces
```

---

## Updated Deliverables for Handoff

Add to the handoff document:

### Additional Deliverable: Algorithm Selection Design

**In ADR 006**, add section:
- Algorithm selection strategy
- Rationale for including in m-3
- m-9 extension path
- Validation approach

**In Configuration Architecture doc**, add section:
- Algorithm enum definition
- Parameter structure extensions
- Per-preset algorithm selection
- Future algorithm roadmap

**In Parameter struct**, add:
- `enum wlblur_algorithm algorithm` field
- Document as "WLBLUR_ALGO_KAWASE only in m-3"
- Reserve space for algorithm-specific params

---

## Summary Answer

**YES**, absolutely include algorithm selection in the TOML config:

```toml
[presets.window]
algorithm = "kawase"  # Add this field now
radius = 8.0
passes = 3
```

**Why:**
1. ✅ Avoid config format breaking change in m-9
2. ✅ Document evolution path clearly
3. ✅ Enable per-preset algorithm selection
4. ✅ Simple to validate (just check == "kawase" in m-3)
5. ✅ Easy to extend (remove validation in m-9)

**Implementation:**
- Add `enum wlblur_algorithm` to blur_params.h (m-3)
- Parse from TOML config (m-3)
- Validate only "kawase" accepted (m-3)
- Remove validation restrictions (m-9)
- Add algorithm dispatch logic (m-9)

This should be explicitly documented in ADR 006 as part of the configuration architecture decision.
