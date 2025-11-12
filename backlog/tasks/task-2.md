---
id: task-2
title: "Extract SceneFX Kawase Blur Shaders"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["shaders", "extraction", "scenefx"]
milestone: "m-1"
dependencies: ["task-1"]
---

## Description

Extract the Dual Kawase blur shaders from SceneFX investigation documentation and create standalone GLSL files for libwlblur. SceneFX is the primary reference because it uses GLES 3.0, has clean structure, and is well-documented.

This task focuses on the **base blur algorithm** (downsample + upsample passes). Post-processing effects (brightness, contrast, saturation, noise) are included if present in SceneFX's blur_effects.frag.

## Acceptance Criteria

- [x] `kawase_downsample.frag.glsl` created with 5-tap sampling
- [x] `kawase_upsample.frag.glsl` created with 8-tap sampling
- [x] `blur_prepare.frag.glsl` created (if exists in SceneFX)
- [x] `blur_finish.frag.glsl` created with post-processing
- [x] `common.glsl` created with shared utility functions
- [x] All shaders use GLES 3.0 syntax (`#version 300 es`)
- [x] All uniforms documented (name, type, purpose, range)
- [x] Sampling patterns explained (ASCII art or comments)
- [x] `libwlblur/shaders/README.md` documents each shader
- [x] Shaders compile with `glslangValidator` (if available)
- [x] Consolidation report created

## Implementation Plan

### Phase 1: Investigation Review
Read from repository:
- `docs/investigation/scenefx-investigation/blur-implementation.md`
- `docs/investigation/scenefx-investigation/api-compatibility.md`

Extract:
- Shader source code (look for GLSL sections)
- Uniform definitions
- Sampling patterns
- Color space handling

### Phase 2: Shader Extraction

#### `kawase_downsample.frag.glsl`
Based on SceneFX `blur1.frag`:
```glsl
#version 300 es
precision mediump float;

uniform sampler2D tex;
uniform vec2 halfpixel;  // (0.5/width, 0.5/height)
uniform float radius;    // Pass index (0, 1, 2, ...)

in vec2 v_texcoord;
out vec4 fragColor;

void main() {
    // 5-tap sampling pattern:
    //   C
    // A   B
    //   D
    //
    // Offset by (radius * 0.5 + 0.5) pixels
    
    vec2 offset = halfpixel * (radius * 0.5 + 0.5);
    
    vec4 sum = texture(tex, v_texcoord) * 4.0;  // Center (weighted)
    sum += texture(tex, v_texcoord + vec2(-offset.x, -offset.y));  // Top-left
    sum += texture(tex, v_texcoord + vec2( offset.x, -offset.y));  // Top-right
    sum += texture(tex, v_texcoord + vec2(-offset.x,  offset.y));  // Bottom-left
    sum += texture(tex, v_texcoord + vec2( offset.x,  offset.y));  // Bottom-right
    
    fragColor = sum / 8.0;
}
```

#### `kawase_upsample.frag.glsl`
Based on SceneFX `blur2.frag`:
```glsl
#version 300 es
precision mediump float;

uniform sampler2D tex;
uniform vec2 halfpixel;
uniform float radius;

in vec2 v_texcoord;
out vec4 fragColor;

void main() {
    // 8-tap sampling pattern:
    //   A   B
    // C   X   D
    //   E   F
    //
    // Offset by radius pixels
    
    vec2 offset = halfpixel * radius;
    
    vec4 sum = texture(tex, v_texcoord) * 4.0;  // Center
    sum += texture(tex, v_texcoord + vec2(-offset.x, -offset.y)) * 2.0;
    sum += texture(tex, v_texcoord + vec2( 0.0,      -offset.y)) * 2.0;
    sum += texture(tex, v_texcoord + vec2( offset.x, -offset.y)) * 2.0;
    sum += texture(tex, v_texcoord + vec2(-offset.x,  0.0))      * 2.0;
    sum += texture(tex, v_texcoord + vec2( offset.x,  0.0))      * 2.0;
    sum += texture(tex, v_texcoord + vec2(-offset.x,  offset.y)) * 2.0;
    sum += texture(tex, v_texcoord + vec2( 0.0,       offset.y)) * 2.0;
    sum += texture(tex, v_texcoord + vec2( offset.x,  offset.y)) * 2.0;
    
    fragColor = sum / 20.0;
}
```

#### `blur_finish.frag.glsl`
Post-processing (brightness, contrast, saturation, noise):
```glsl
#version 300 es
precision mediump float;

uniform sampler2D tex;
uniform float brightness;   // 0.0-2.0, default 0.9
uniform float contrast;     // 0.0-2.0, default 0.9
uniform float saturation;   // 0.0-2.0, default 1.1
uniform float noise;        // 0.0-0.1, default 0.02

in vec2 v_texcoord;
out vec4 fragColor;

// Pseudo-random noise function
float rand(vec2 co) {
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

void main() {
    vec4 color = texture(tex, v_texcoord);
    
    // Brightness
    color.rgb *= brightness;
    
    // Contrast (around 0.5 gray)
    color.rgb = (color.rgb - 0.5) * contrast + 0.5;
    
    // Saturation
    float gray = dot(color.rgb, vec3(0.299, 0.587, 0.114));
    color.rgb = mix(vec3(gray), color.rgb, saturation);
    
    // Noise
    if (noise > 0.0) {
        float n = (rand(v_texcoord) - 0.5) * noise;
        color.rgb += n;
    }
    
    fragColor = color;
}
```

### Phase 3: Documentation

Create `libwlblur/shaders/README.md`:
```markdown
# libwlblur Shaders

Blur shaders extracted from SceneFX and optimized for standalone use.

## kawase_downsample.frag.glsl
**Purpose**: Downsample pass of Dual Kawase blur

**Uniforms**:
- `sampler2D tex`: Input texture
- `vec2 halfpixel`: Half-pixel offset (0.5/width, 0.5/height)
- `float radius`: Sampling radius (typically pass index: 0, 1, 2, ...)

**Algorithm**: 5-tap cross pattern
```
    C
  A   B
    D
```

**Source**: SceneFX blur1.frag

## kawase_upsample.frag.glsl
[Similar format for each shader]
```

Create `docs/consolidation/shader-extraction-scenefx.md`:
```markdown
# SceneFX Shader Extraction Report

## Source
- Investigation: `docs/investigation/scenefx-investigation/`
- Original: https://github.com/wlrfx/scenefx
- Shaders: blur1.frag, blur2.frag, blur_effects.frag

## Extraction Process
1. Reviewed blur-implementation.md for shader code
2. Copied GLSL source verbatim
3. Updated to GLES 3.0 ES (from whatever SceneFX uses)
4. Removed wlroots-specific code (if any)
5. Added extensive comments

## Changes Made
- Standardized to `#version 300 es`
- Renamed uniforms for clarity (if needed)
- Added ASCII art for sampling patterns
- Split monolithic shader into modular files

## Validation
- [x] Shaders compile with glslangValidator
- [x] Uniforms match expected types
- [x] Sampling patterns preserved
- [x] No vendor-specific extensions

## Known Issues
[None expected, but document any concerns]

## Next Steps
- Extract Hyprland vibrancy shader (task-3)
- Consolidate parameter schema (task-4)
```

## Notes & Comments

**Shader Compiler**: Use `glslangValidator` if available:
```bash
glslangValidator -V kawase_downsample.frag.glsl
```

**Vertex Shader**: If SceneFX uses custom vertex shader, extract it. Otherwise, libwlblur will use a standard fullscreen quad shader.

**SceneFX License**: Ensure SceneFX shader headers are preserved if required by their license.

**ASCII Art**: Use ASCII diagrams to visualize sampling patterns - very helpful for understanding the algorithm.

**Deliverables**:
1. All shader files in `libwlblur/shaders/`
2. `README.md` in shaders directory
3. Consolidation report in `docs/consolidation/`
4. List of any assumptions or deviations
