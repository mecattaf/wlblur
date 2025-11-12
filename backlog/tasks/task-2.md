id: task-2
title: "Extract Complete Shader Set from SceneFX, Hyprland, and Wayfire"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["shaders", "extraction", "scenefx", "hyprland", "wayfire"]
milestone: "m-1"
dependencies: ["task-1"]
---

## Description

Extract ALL blur shaders from the three reference implementations (SceneFX, Hyprland, Wayfire) into a unified, standalone shader set for libwlblur. This creates the complete GLSL shader library that implements:
- Dual Kawase blur (from SceneFX - base implementation)
- Vibrancy/HSL boost (from Hyprland - color enhancement)
- Post-processing effects (brightness, contrast, saturation, noise)

**This task can run in parallel with task-3 after task-1 completes.**

## Acceptance Criteria

- [x] SceneFX shaders extracted to libwlblur/shaders/
- [x] Hyprland vibrancy shader extracted
- [x] All shaders use GLSL 3.0 ES (`#version 300 es`)
- [x] All uniforms documented (name, type, purpose, range)
- [x] Sampling patterns explained with ASCII art or comments
- [x] libwlblur/shaders/README.md documents each shader
- [x] Shaders compile with glslangValidator (if available)
- [x] docs/consolidation/shader-extraction-report.md created
- [x] Copyright headers preserve original licenses
- [x] All changes from originals documented

## Implementation Plan

### Phase 0: Clone Source Repositories

```bash
cd /tmp
git clone https://github.com/wlrfx/scenefx
git clone https://github.com/hyprwm/Hyprland
git clone https://github.com/WayfireWM/wayfire

# Optional: Check out specific stable versions
cd scenefx && git checkout v0.1.0  # or latest stable tag
cd ../Hyprland && git checkout v0.34.0  # or latest stable
cd ../wayfire && git checkout v0.8.0  # or latest stable
```

### Phase 1: Extract SceneFX Base Shaders

**Source files:**
- `/tmp/scenefx/render/fx_renderer/gles3/shaders/blur1.frag`
- `/tmp/scenefx/render/fx_renderer/gles3/shaders/blur2.frag`
- `/tmp/scenefx/render/fx_renderer/gles3/shaders/blur_effects.frag`

**Target files:**
- `libwlblur/shaders/kawase_downsample.frag.glsl` ← blur1.frag
- `libwlblur/shaders/kawase_upsample.frag.glsl` ← blur2.frag
- `libwlblur/shaders/blur_finish.frag.glsl` ← blur_effects.frag

**Extraction process for each shader:**

1. Copy shader source verbatim
2. Ensure `#version 300 es` at top
3. Add comprehensive header comment:
```glsl
/*
 * Kawase Downsample Shader
 * 
 * Extracted from SceneFX (MIT License)
 * Copyright (c) 2023 Erik Reider, Scott Moreau
 * https://github.com/wlrfx/scenefx
 * 
 * Original file: render/fx_renderer/gles3/shaders/blur1.frag
 * 
 * Modifications for wlblur:
 * - Renamed to kawase_downsample.frag.glsl
 * - Added detailed uniform documentation
 * - [List any other changes]
 * 
 * SPDX-License-Identifier: MIT
 */

#version 300 es
precision mediump float;

// Input texture to blur
uniform sampler2D tex;

// Half-pixel offset: vec2(0.5/width, 0.5/height)
// Used to sample between pixels for smoother results
uniform vec2 halfpixel;

// Sampling radius: typically pass index (0, 1, 2, ...)
// Determines how far from center to sample
// Actual offset = halfpixel * (radius * 0.5 + 0.5)
uniform float radius;

// Texture coordinates from vertex shader
in vec2 v_texcoord;

// Output color
out vec4 fragColor;

/*
 * Kawase Downsample: 5-tap sampling pattern
 * 
 * Sampling pattern (X = center):
 *     A
 *   D   B
 *     C
 * 
 * Center weighted 4x, corners weighted 1x each
 * Total weight: 8 (center=4, corners=4)
 */
void main() {
    vec2 offset = halfpixel * (radius * 0.5 + 0.5);
    
    // Center sample (weight 4)
    vec4 sum = texture(tex, v_texcoord) * 4.0;
    
    // Four corner samples (weight 1 each)
    sum += texture(tex, v_texcoord + vec2(-offset.x, -offset.y));  // Top-left
    sum += texture(tex, v_texcoord + vec2( offset.x, -offset.y));  // Top-right
    sum += texture(tex, v_texcoord + vec2(-offset.x,  offset.y));  // Bottom-left
    sum += texture(tex, v_texcoord + vec2( offset.x,  offset.y));  // Bottom-right
    
    fragColor = sum / 8.0;
}
```

4. Document ANY changes made from original:
   - If uniform names changed: document
   - If sampling pattern modified: document and justify
   - If precision changed: document

**Repeat for blur2.frag (upsample) and blur_effects.frag (post-processing).**

**Key details for upsample shader:**
- 8-tap pattern (4 cardinal + 4 diagonal)
- Weighted blending (diagonal samples weighted 2x)
- Total weight: 12

**Key details for blur_effects shader:**
- Brightness adjustment (multiply RGB)
- Contrast adjustment (around 0.5 gray point)
- Saturation adjustment (RGB → gray → mix based on saturation)
- Noise addition (pseudo-random per-pixel)

### Phase 2: Extract Hyprland Vibrancy Shader

**Source files:**
- `/tmp/Hyprland/src/render/shaders/glsl/blur1.frag` (check for vibrancy code)
- `/tmp/Hyprland/src/render/shaders/glsl/blurfinish.frag` (likely location)
- `/tmp/Hyprland/src/render/shaders/glsl/CM.glsl` (color management functions)

**Target file:**
- `libwlblur/shaders/vibrancy.frag.glsl`

**What to extract:**

RGB ↔ HSL conversion functions:
```glsl
vec3 rgb2hsl(vec3 rgb) {
    // [Extract from Hyprland]
    // Convert RGB [0,1] to HSL [0-360, 0-1, 0-1]
}

vec3 hsl2rgb(vec3 hsl) {
    // [Extract from Hyprland]
    // Convert HSL back to RGB
}
```

Vibrancy application:
```glsl
void main() {
    vec4 color = texture(tex, v_texcoord);
    
    if (vibrancy_strength > 0.0) {
        vec3 hsl = rgb2hsl(color.rgb);
        
        // Boost saturation
        hsl.y *= (1.0 + vibrancy_strength);
        hsl.y = clamp(hsl.y, 0.0, 1.0);
        
        // Optionally darken to prevent washout
        hsl.z *= (1.0 - vibrancy_darkness);
        
        color.rgb = hsl2rgb(hsl);
    }
    
    fragColor = color;
}
```

**Header format:**
```glsl
/*
 * Vibrancy Shader (HSL Color Boost)
 * 
 * Extracted from Hyprland (BSD-3-Clause License)
 * Copyright (c) 2022-2023 Vaxry
 * https://github.com/hyprwm/Hyprland
 * 
 * Original files:
 * - src/render/shaders/glsl/blurfinish.frag (vibrancy application)
 * - src/render/shaders/glsl/CM.glsl (HSL conversion)
 * 
 * Modifications for wlblur:
 * - Extracted as standalone shader
 * - GLSL 3.0 ES compatibility
 * - Explicit enable/disable via uniform
 * 
 * SPDX-License-Identifier: BSD-3-Clause
 */
```

### Phase 3: Create Common Utilities Shader

**Target file:**
- `libwlblur/shaders/common.glsl`

**Contents:**
- Shared utility functions used by multiple shaders
- Pseudo-random number generation (for noise)
- Color space conversion helpers
- Sampling pattern generators

**Example:**
```glsl
/*
 * Common Shader Utilities
 * 
 * Shared functions for wlblur shaders
 * 
 * SPDX-License-Identifier: MIT
 */

#ifndef WLBLUR_COMMON_GLSL
#define WLBLUR_COMMON_GLSL

// Pseudo-random number generator
// Input: 2D seed (e.g., texture coordinates)
// Output: Random float [0, 1]
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// Luminance calculation (ITU-R BT.709)
float luminance(vec3 rgb) {
    return dot(rgb, vec3(0.2126, 0.7152, 0.0722));
}

// Grayscale conversion
vec3 toGrayscale(vec3 rgb) {
    float gray = luminance(rgb);
    return vec3(gray);
}

#endif // WLBLUR_COMMON_GLSL
```

### Phase 4: Shader Documentation

**libwlblur/shaders/README.md:**
```markdown
# wlblur Shaders

Blur shaders extracted from SceneFX, Hyprland, and Wayfire.

## Shader Files

### kawase_downsample.frag.glsl
**Purpose**: First pass of Dual Kawase blur (downsample)

**Uniforms**:
| Name | Type | Description | Range |
|------|------|-------------|-------|
| tex | sampler2D | Input texture to blur | - |
| halfpixel | vec2 | Half-pixel offset (0.5/width, 0.5/height) | - |
| radius | float | Sampling radius (typically pass index) | 0-8 |

**Algorithm**: 5-tap cross pattern
```
    A
  D   B
    C
```
Center weighted 4x, corners 1x each. Total weight: 8.

**Source**: SceneFX blur1.frag (MIT License)

**Performance**: ~0.3ms per pass @ 1080p

---

### kawase_upsample.frag.glsl
**Purpose**: Second pass of Dual Kawase blur (upsample)

**Uniforms**:
| Name | Type | Description | Range |
|------|------|-------------|-------|
| tex | sampler2D | Downsampled texture | - |
| halfpixel | vec2 | Half-pixel offset | - |
| radius | float | Sampling radius | 0-8 |

**Algorithm**: 8-tap pattern (4 cardinal + 4 diagonal)
```
  A   B   C
  D   X   E
  F   G   H
```
Cardinal + diagonal samples weighted 2x. Total weight: 12.

**Source**: SceneFX blur2.frag (MIT License)

**Performance**: ~0.4ms per pass @ 1080p

---

### blur_finish.frag.glsl
**Purpose**: Post-processing effects (brightness, contrast, saturation, noise)

**Uniforms**:
| Name | Type | Description | Range | Default |
|------|------|-------------|-------|---------|
| tex | sampler2D | Blurred texture | - | - |
| brightness | float | Brightness multiplier | 0.0-2.0 | 0.9 |
| contrast | float | Contrast adjustment | 0.0-2.0 | 0.9 |
| saturation | float | Saturation adjustment | 0.0-2.0 | 1.1 |
| noise | float | Noise amount | 0.0-0.1 | 0.02 |

**Source**: SceneFX blur_effects.frag (MIT License)

**Performance**: ~0.2ms @ 1080p

---

### vibrancy.frag.glsl (Phase 7 Feature)
**Purpose**: HSL-based color boost for macOS-style vibrancy

**Uniforms**:
| Name | Type | Description | Range | Default |
|------|------|-------------|-------|---------|
| tex | sampler2D | Input texture | - | - |
| vibrancy_strength | float | Saturation multiplier | 0.0-2.0 | 0.0 |
| vibrancy_darkness | float | Lightness reduction | 0.0-1.0 | 0.0 |

**Algorithm**: RGB → HSL → boost saturation → RGB

**Source**: Hyprland blurfinish.frag (BSD-3-Clause License)

**Performance**: ~0.15ms @ 1080p

**Note**: vibrancy_strength=0.0 makes this a passthrough (no-op).

---

### common.glsl
**Purpose**: Shared utility functions

**Functions**:
- `float rand(vec2)` - Pseudo-random number generation
- `float luminance(vec3)` - ITU-R BT.709 luminance
- `vec3 toGrayscale(vec3)` - Convert to grayscale

**Source**: wlblur original

---

## Usage Example

```c
// Load shaders
GLuint downsample_shader = load_shader("kawase_downsample.frag.glsl");
GLuint upsample_shader = load_shader("kawase_upsample.frag.glsl");
GLuint finish_shader = load_shader("blur_finish.frag.glsl");

// Multi-pass blur
for (int pass = 0; pass < num_passes; pass++) {
    // Downsample
    glUseProgram(downsample_shader);
    glUniform2f(halfpixel_loc, 0.5/width, 0.5/height);
    glUniform1f(radius_loc, pass);
    render_fullscreen_quad();
}

for (int pass = num_passes-1; pass >= 0; pass--) {
    // Upsample
    glUseProgram(upsample_shader);
    glUniform2f(halfpixel_loc, 0.5/width, 0.5/height);
    glUniform1f(radius_loc, pass);
    render_fullscreen_quad();
}

// Post-processing
glUseProgram(finish_shader);
glUniform1f(brightness_loc, 0.9);
glUniform1f(contrast_loc, 0.9);
glUniform1f(saturation_loc, 1.1);
glUniform1f(noise_loc, 0.02);
render_fullscreen_quad();
```

## Shader Validation

All shaders validated with:
```bash
glslangValidator -V kawase_downsample.frag.glsl
glslangValidator -V kawase_upsample.frag.glsl
glslangValidator -V blur_finish.frag.glsl
glslangValidator -V vibrancy.frag.glsl
```

## License Attribution

- SceneFX shaders: MIT License
- Hyprland shaders: BSD-3-Clause License
- wlblur additions: MIT License

See individual shader headers for complete copyright information.
```

### Phase 5: Consolidation Report

**docs/consolidation/shader-extraction-report.md:**
```markdown
# Shader Extraction Report

## Executive Summary

Extracted complete shader set from three production Wayland compositors:
- SceneFX: Base Dual Kawase implementation (4 shaders)
- Hyprland: Vibrancy enhancement (1 shader)
- Common utilities: wlblur original (1 shader)

**Total**: 6 shaders, ~400 lines of GLSL

## Extraction Sources

### SceneFX (MIT License)
**Repository**: https://github.com/wlrfx/scenefx  
**Commit**: [Record commit hash used]  
**Files extracted**:
1. `render/fx_renderer/gles3/shaders/blur1.frag` → `kawase_downsample.frag.glsl`
2. `render/fx_renderer/gles3/shaders/blur2.frag` → `kawase_upsample.frag.glsl`
3. `render/fx_renderer/gles3/shaders/blur_effects.frag` → `blur_finish.frag.glsl`

**Changes made**:
- Renamed files for clarity
- Added comprehensive header documentation
- Ensured GLSL 3.0 ES compatibility
- [List any algorithmic changes]

### Hyprland (BSD-3-Clause License)
**Repository**: https://github.com/hyprwm/Hyprland  
**Commit**: [Record commit hash used]  
**Files extracted**:
1. `src/render/shaders/glsl/blurfinish.frag` (vibrancy code) → `vibrancy.frag.glsl`
2. `src/render/shaders/glsl/CM.glsl` (HSL conversion) → `vibrancy.frag.glsl`

**Changes made**:
- Extracted vibrancy as standalone shader
- Ensured GLSL 3.0 ES compatibility
- Made vibrancy explicitly opt-in (strength=0 disables)
- [List any color space conversion modifications]

## Validation

### Compilation Tests
```bash
# All shaders compile without errors
glslangValidator -V libwlblur/shaders/*.glsl
# Result: 6/6 shaders pass
```

### Visual Comparison
[Describe any visual comparison tests against original implementations]

### Performance Baseline
| Shader | Resolution | Time |
|--------|------------|------|
| Downsample | 1080p | ~0.3ms |
| Upsample | 1080p | ~0.4ms |
| Finish | 1080p | ~0.2ms |
| Vibrancy | 1080p | ~0.15ms |

**Total pipeline**: ~1.2ms for 3-pass Kawase + effects @ 1080p

## License Compliance

✅ All original copyright notices preserved  
✅ License files included (LICENSE.SceneFX, LICENSE.Hyprland)  
✅ Attribution in shader headers  
✅ No GPL contamination  
✅ Modifications documented

## Known Differences from Originals

### SceneFX Shaders
- **Uniform precision**: Changed from highp to mediump for mobile compatibility
- **Variable names**: [Document any renaming]
- **Sampling patterns**: Preserved exactly (verified)

### Hyprland Vibrancy
- **Extraction**: Isolated from larger blur pipeline
- **HSL conversion**: Extracted from CM.glsl color management module
- **Default behavior**: strength=0 makes it passthrough (original always applies)

## Integration Notes

### For libwlblur Implementation
- Shaders expect standard GL context (GLES 3.0+)
- Uniforms must be set before each draw call
- Framebuffer ping-pong required for multi-pass
- See libwlblur/shaders/README.md for usage examples

### For Upstream Contributions
If bugs found in extracted shaders:
1. Create minimal reproducer
2. Test against original implementation
3. Report to upstream (SceneFX or Hyprland)
4. Credit upstream in fix

## Future Work

### Additional Algorithms (Wayfire)
Phase 2 will extract from Wayfire:
- Gaussian blur
- Box blur
- Bokeh blur

Wayfire uses GLSL 2.0 ES, will require compatibility work.

### Optimizations
- Half-float textures (GL_RGBA16F)
- Compute shader variants (GLES 3.1+)
- Shader compilation caching

## References

- ADR-003: Kawase algorithm choice
- ADR-005: SceneFX extraction rationale
- Investigation docs: docs/investigation/scenefx-investigation/
- Investigation docs: docs/investigation/hyprland-investigation/

## Sign-off

Shader extraction completed on: [Date]  
Validated by: @claude-code  
Reviewed by: @mecattaf  
License compliance checked: ✅  
Visual quality validated: ✅  
Performance within target: ✅
```

## Validation Commands

```bash
# Validate all shaders compile
for shader in libwlblur/shaders/*.glsl; do
    echo "Validating $shader..."
    glslangValidator -V "$shader" || echo "FAIL: $shader"
done

# Check copyright headers present
grep -r "SPDX-License-Identifier" libwlblur/shaders/ | wc -l
# Should equal number of .glsl files

# Verify no hardcoded shader code in .c files (should be loaded from files)
grep -r "#version" libwlblur/src/ && echo "WARNING: Shader code in source files"
```

## Notes & Comments

**Shader Portability**: All shaders use GLES 3.0 (2012 standard). Very broad compatibility.

**License Mixing**: MIT (SceneFX) and BSD-3-Clause (Hyprland) are compatible. Both permissive.

**Wayfire Extraction**: Deferred to future phase. Wayfire uses GLSL 2.0 ES, needs separate extraction effort.

**Header Documentation**: Extensive comments added to make shaders self-documenting. Original implementations assume compositor context.

**Testing**: Visual comparison against reference implementations recommended but not blocking for task completion.

## Deliverables

1. All shader files in libwlblur/shaders/
2. Comprehensive libwlblur/shaders/README.md
3. docs/consolidation/shader-extraction-report.md
4. Commit with message: "feat(shaders): extract complete shader set from SceneFX and Hyprland"
