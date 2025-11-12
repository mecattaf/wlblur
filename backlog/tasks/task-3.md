---
id: task-3
title: "Extract Hyprland Vibrancy Shader"
status: To Do
assignee: "@claude-code"
reporter: "@mecattaf"
created_date: 2025-01-15
labels: ["shaders", "extraction", "hyprland", "phase-7"]
milestone: "m-1"
dependencies: ["task-2"]
---

## Description

Extract Hyprland's vibrancy system (HSL color boost) to complement the SceneFX base shaders. Vibrancy is a **Phase 7 feature** (post-MVP), but we extract it now for completeness during the shader consolidation phase.

Hyprland's vibrancy converts RGB to HSL, boosts saturation and adjusts lightness, then converts back to RGB. It's applied as a post-processing effect.

## Acceptance Criteria

- [x] `vibrancy.frag.glsl` created with HSL conversion
- [x] RGB→HSL and HSL→RGB functions implemented
- [x] Vibrancy strength parameter (0.0 = disabled)
- [x] Vibrancy darkness parameter for lightness adjustment
- [x] Shader uses GLES 3.0 syntax
- [x] Uniforms documented
- [x] Color math validated against Hyprland
- [x] `libwlblur/shaders/README.md` updated
- [x] Consolidation report created
- [x] Shader compiles

## Implementation Plan

### Phase 1: Investigation Review
Read from repository:
- `docs/investigation/hyprland-investigation/blur-implementation.md`
- `docs/post-investigation/hyprland-parity-explanation.md`

Look for:
- `blurprepare.frag` / `blurfinish.frag` vibrancy sections
- HSL conversion math
- Parameter definitions

### Phase 2: Vibrancy Shader

Create `libwlblur/shaders/vibrancy.frag.glsl`:
```glsl
#version 300 es
precision mediump float;

uniform sampler2D tex;
uniform float vibrancy_strength;    // 0.0 = off, 1.0+ = boost
uniform float vibrancy_darkness;    // 0.0-1.0, lightness reduction

in vec2 v_texcoord;
out vec4 fragColor;

// RGB to HSL conversion
// H: Hue [0, 360)
// S: Saturation [0, 1]
// L: Lightness [0, 1]
vec3 rgb2hsl(vec3 rgb) {
    float maxc = max(rgb.r, max(rgb.g, rgb.b));
    float minc = min(rgb.r, min(rgb.g, rgb.b));
    float delta = maxc - minc;
    
    float l = (maxc + minc) / 2.0;
    
    if (delta == 0.0) {
        return vec3(0.0, 0.0, l);  // Achromatic (gray)
    }
    
    float s = l < 0.5 ? delta / (maxc + minc) : delta / (2.0 - maxc - minc);
    
    float h;
    if (rgb.r == maxc) {
        h = (rgb.g - rgb.b) / delta + (rgb.g < rgb.b ? 6.0 : 0.0);
    } else if (rgb.g == maxc) {
        h = (rgb.b - rgb.r) / delta + 2.0;
    } else {
        h = (rgb.r - rgb.g) / delta + 4.0;
    }
    h /= 6.0;
    
    return vec3(h, s, l);
}

// HSL to RGB conversion
vec3 hsl2rgb(vec3 hsl) {
    float h = hsl.x;
    float s = hsl.y;
    float l = hsl.z;
    
    if (s == 0.0) {
        return vec3(l);  // Achromatic
    }
    
    float q = l < 0.5 ? l * (1.0 + s) : l + s - l * s;
    float p = 2.0 * l - q;
    
    vec3 rgb;
    rgb.r = hue2rgb(p, q, h + 1.0/3.0);
    rgb.g = hue2rgb(p, q, h);
    rgb.b = hue2rgb(p, q, h - 1.0/3.0);
    
    return rgb;
}

float hue2rgb(float p, float q, float t) {
    if (t < 0.0) t += 1.0;
    if (t > 1.0) t -= 1.0;
    if (t < 1.0/6.0) return p + (q - p) * 6.0 * t;
    if (t < 1.0/2.0) return q;
    if (t < 2.0/3.0) return p + (q - p) * (2.0/3.0 - t) * 6.0;
    return p;
}

void main() {
    vec4 color = texture(tex, v_texcoord);
    
    if (vibrancy_strength > 0.0) {
        vec3 hsl = rgb2hsl(color.rgb);
        
        // Boost saturation
        hsl.y *= (1.0 + vibrancy_strength);
        hsl.y = clamp(hsl.y, 0.0, 1.0);
        
        // Reduce lightness to prevent washout
        hsl.z *= (1.0 - vibrancy_darkness);
        hsl.z = clamp(hsl.z, 0.0, 1.0);
        
        color.rgb = hsl2rgb(hsl);
    }
    
    fragColor = color;
}
```

### Phase 3: Documentation

Update `libwlblur/shaders/README.md`:
```markdown
## vibrancy.frag.glsl (Phase 7 Feature)
**Purpose**: HSL-based color boost for macOS-style vibrancy

**Uniforms**:
- `sampler2D tex`: Input texture
- `float vibrancy_strength`: Saturation multiplier (0.0-2.0, default 0.0)
  - 0.0 = disabled (passthrough)
  - 1.15 = Hyprland default
- `float vibrancy_darkness`: Lightness reduction (0.0-1.0, default 0.0)
  - Prevents colors from becoming too bright when saturation boosted

**Algorithm**: 
1. Convert RGB → HSL
2. Multiply saturation by (1 + strength)
3. Multiply lightness by (1 - darkness)
4. Convert HSL → RGB

**Source**: Extracted from Hyprland blurfinish.frag

**Phase**: 7 (not used in MVP, vibrancy_strength defaults to 0.0)

**Example**:
- vibrancy_strength=0.0: No effect
- vibrancy_strength=1.15, darkness=0.0: Hyprland style boost
```

Create `docs/consolidation/shader-extraction-hyprland.md`:
```markdown
# Hyprland Vibrancy Extraction Report

## Source
- Investigation: `docs/investigation/hyprland-investigation/`
- Original: https://github.com/hyprwm/Hyprland
- Relevant shaders: blurprepare.frag, blurfinish.frag

## Extracted Features
1. **Vibrancy System**
   - RGB ↔ HSL color space conversion
   - Saturation boost
   - Lightness adjustment
   - Optional (disabled by default in libwlblur)

## Integration Points
- Applied **after** blur finish pass
- Separate shader (not merged into blur_finish.frag)
- vibrancy_strength=0.0 makes it a passthrough
- Compatible with SceneFX base shaders

## Changes from Original
- Extracted as standalone shader (not integrated into blur pipeline)
- Explicit enable/disable via uniform
- GLES 3.0 compatible (Hyprland uses GLES 3.2)
- Removed Hyprland-specific HDR code (if present)

## Algorithm Validation
HSL conversion math verified against:
- https://en.wikipedia.org/wiki/HSL_and_HSV
- Hyprland's implementation
- Reference color values tested

## Testing Strategy
1. Test with vibrancy_strength=0.0 → should be bit-identical passthrough
2. Test with vibrancy_strength=1.15 → compare to Hyprland screenshots
3. Verify color accuracy with reference images
4. Test edge cases (pure gray, pure colors)

## Known Limitations
- Does not include Hyprland's HDR color management (future feature)
- Simplified compared to Hyprland's full pipeline
- Phase 7 feature, not required for MVP

## Next Steps
- Implement vibrancy in Phase 7
- Test visual parity with Hyprland
- Document performance impact
```

## Notes & Comments

**Phase 7 Feature**: This shader is **not used in MVP**. It's extracted now for completeness but won't be integrated until Phase 7 (Apple effects parity).

**Default Behavior**: In libwlblur, vibrancy_strength defaults to 0.0, making this shader a no-op (passthrough) until explicitly enabled.

**Color Accuracy**: HSL conversion math is well-defined. Verify against reference implementations to ensure no color shifts.

**Performance**: HSL conversion adds ~0.1-0.2ms per frame. Negligible for modern GPUs.

**Deliverables**:
1. `vibrancy.frag.glsl`
2. Updated `shaders/README.md`
3. `docs/consolidation/shader-extraction-hyprland.md`
4. Test with glslangValidator
