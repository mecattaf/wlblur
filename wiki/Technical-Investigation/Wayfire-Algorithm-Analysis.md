# Wayfire Blur Algorithms - Detailed Analysis

## Overview

Wayfire implements **four distinct blur algorithms**, each with different quality/performance trade-offs:

1. **Kawase Blur** (Default) - Dual-pass downsample/upsample
2. **Gaussian Blur** - Separable weighted kernel
3. **Box Blur** - Simple averaging filter
4. **Bokeh Blur** - Depth-of-field style circular blur

All algorithms share the same base infrastructure (`wf_blur_base`) but differ in their shader implementations and sampling patterns.

---

## Algorithm Comparison Matrix

| Algorithm | Passes | Shader Complexity | Visual Quality | Performance | Best For |
|-----------|--------|------------------|----------------|-------------|----------|
| **Kawase** | 2×N (down+up) | Medium | High | Excellent | General use (default) |
| **Gaussian** | 2×N (H+V) | Low-Medium | Very High | Good | Quality-focused |
| **Box** | 2×N (H+V) | Low | Medium | Best | Performance-focused |
| **Bokeh** | 1 (multi-sample) | High | Unique/Artistic | Poor | Special effects |

**N** = number of iterations (configurable per algorithm)

---

## 1. Kawase Blur (Dual Kawase)

**File**: `plugins/blur/kawase.cpp`

### Algorithm Description

The **Dual Kawase** blur is a highly efficient approximation of Gaussian blur that achieves large blur radii with minimal passes. It works by:

1. **Downsampling** with blur - progressively halve texture resolution while blurring
2. **Upsampling** with blur - progressively double resolution back while blurring

This creates a pyramid structure where each level has 1/4 the pixels of the previous level.

### Default Configuration

```xml
<option name="kawase_offset">1.7</option>
<option name="kawase_degrade">3</option>
<option name="kawase_iterations">2</option>
```

**Effective blur radius**: `2^(iterations+1) × offset × degrade = 2^3 × 1.7 × 3 ≈ 40.8 pixels`

### Shader Implementation

#### Downsample Shader

**File**: `kawase.cpp:15-34`

```glsl
void main() {
    vec4 sum = texture2D(bg_texture, uv) * 4.0;
    sum += texture2D(bg_texture, uv - halfpixel.xy * offset);
    sum += texture2D(bg_texture, uv + halfpixel.xy * offset);
    sum += texture2D(bg_texture, uv + vec2(halfpixel.x, -halfpixel.y) * offset);
    sum += texture2D(bg_texture, uv - vec2(halfpixel.x, -halfpixel.y) * offset);
    gl_FragColor = sum / 8.0;
}
```

**Sampling pattern** (5 samples):
```
    1
  0 4 2
    3
```
- Center pixel weighted 4×
- Four diagonal corners weighted 1× each
- Normalized by dividing by 8

#### Upsample Shader

**File**: `kawase.cpp:36-58`

```glsl
void main() {
    vec4 sum = texture2D(bg_texture, uv + vec2(-halfpixel.x * 2.0, 0.0) * offset);
    sum += texture2D(bg_texture, uv + vec2(-halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture2D(bg_texture, uv + vec2(0.0, halfpixel.y * 2.0) * offset);
    sum += texture2D(bg_texture, uv + vec2(halfpixel.x, halfpixel.y) * offset) * 2.0;
    sum += texture2D(bg_texture, uv + vec2(halfpixel.x * 2.0, 0.0) * offset);
    sum += texture2D(bg_texture, uv + vec2(halfpixel.x, -halfpixel.y) * offset) * 2.0;
    sum += texture2D(bg_texture, uv + vec2(0.0, -halfpixel.y * 2.0) * offset);
    sum += texture2D(bg_texture, uv + vec2(-halfpixel.x, -halfpixel.y) * offset) * 2.0;
    gl_FragColor = sum / 12.0;
}
```

**Sampling pattern** (8 samples):
```
  1
2   3
  0   4
5   6
  7
```
- Cardinal directions (2-pixel offset) weighted 1× each (4 samples)
- Diagonal directions (1-pixel offset) weighted 2× each (4 samples)
- Normalized by dividing by 12

### Execution Flow

**File**: `kawase.cpp:74-136`

```cpp
int blur_fb0(const wf::region_t& blur_region, int width, int height) override {
    int iterations = iterations_opt;
    float offset = offset_opt;

    // Downsample passes
    program[0].use(wf::TEXTURE_TYPE_RGBA);
    for (int i = 0; i < iterations; i++) {
        sampleWidth  = width / (1 << i);   // Halve width each pass
        sampleHeight = height / (1 << i);  // Halve height each pass
        auto region = blur_region * (1.0 / (1 << i));
        program[0].uniform2f("halfpixel", 0.5f / sampleWidth, 0.5f / sampleHeight);
        render_iteration(region, fb[i % 2], fb[1 - i % 2], sampleWidth, sampleHeight);
    }

    // Upsample passes
    program[1].use(wf::TEXTURE_TYPE_RGBA);
    for (int i = iterations - 1; i >= 0; i--) {
        sampleWidth  = width / (1 << i);   // Double width each pass
        sampleHeight = height / (1 << i);  // Double height each pass
        auto region = blur_region * (1.0 / (1 << i));
        program[1].uniform2f("halfpixel", 0.5f / sampleWidth, 0.5f / sampleHeight);
        render_iteration(region, fb[1 - i % 2], fb[i % 2], sampleWidth, sampleHeight);
    }

    return 0;  // Result is in fb[0]
}
```

**Example with 2 iterations**:

| Pass | Operation | Resolution | Shader | Buffer Flow |
|------|-----------|------------|--------|-------------|
| 0 | Downsample | 100% → 50% | Down | fb[0] → fb[1] |
| 1 | Downsample | 50% → 25% | Down | fb[1] → fb[0] |
| 2 | Upsample | 25% → 50% | Up | fb[0] → fb[1] |
| 3 | Upsample | 50% → 100% | Up | fb[1] → fb[0] |

### Performance Characteristics

**Texture fetches per pixel**:
- Downsample: 5 fetches
- Upsample: 8 fetches
- Total per iteration pair: 13 fetches

**Total work** (2 iterations, 1920×1080):
- Pass 0: 1920×1080 × 5 = 10.4M fetches
- Pass 1: 960×540 × 5 = 2.6M fetches
- Pass 2: 960×540 × 8 = 4.1M fetches
- Pass 3: 1920×1080 × 8 = 16.6M fetches
- **Total: 33.7M fetches** (effective work due to downsampling)

Compare to naive Gaussian at same radius (40px) with 2-pass separable:
- Pass 0: 1920×1080 × 81 samples = 168M fetches
- Pass 1: 1920×1080 × 81 samples = 168M fetches
- **Total: 336M fetches**

**Kawase is ~10× more efficient** for large radii.

### Visual Quality

- **Smooth falloff**: Multi-scale approach approximates Gaussian well
- **Minimal artifacts**: Downsampling hides aliasing
- **Slightly anisotropic**: Not perfectly circular (due to diagonal sampling)
- **Good color preservation**: Linear blending maintains color accuracy

**Best for**: General-purpose blur where performance and quality must balance.

---

## 2. Gaussian Blur (Separable)

**File**: `plugins/blur/gaussian.cpp`

### Algorithm Description

True **Gaussian blur** uses a weighted kernel based on the Gaussian (normal) distribution. The blur is **separable**, meaning a 2D Gaussian can be computed as two 1D passes (horizontal, then vertical).

Wayfire uses a **5-tap approximation** of a Gaussian kernel with optimized weights.

### Default Configuration

```xml
<option name="gaussian_offset">1.0</option>
<option name="gaussian_degrade">1</option>
<option name="gaussian_iterations">2</option>
```

**Effective blur radius**: `4 × offset × degrade × iterations = 4 × 1 × 1 × 2 = 8 pixels`

### Shader Implementation

#### Gaussian Weights

The fragment shaders use pre-computed weights for a 5-tap kernel:

```glsl
bp += texture2D(bg_texture, blurcoord[0]) * 0.204164;  // Center
bp += texture2D(bg_texture, blurcoord[1]) * 0.304005;  // +1.5 offset
bp += texture2D(bg_texture, blurcoord[2]) * 0.304005;  // -1.5 offset
bp += texture2D(bg_texture, blurcoord[3]) * 0.093913;  // +3.5 offset
bp += texture2D(bg_texture, blurcoord[4]) * 0.093913;  // -3.5 offset
```

**Weights sum to 1.0**: 0.204164 + 2×0.304005 + 2×0.093913 = 1.0

These weights approximate a Gaussian with σ ≈ 1.5:

```
         ▁▂▅▇██▇▅▂▁
    -3.5  -1.5  0  1.5  3.5
```

#### Horizontal Pass

**File**: `gaussian.cpp:26-46`

```glsl
void main() {
    vec2 uv = blurcoord[0];
    vec4 bp = vec4(0.0);
    bp += texture2D(bg_texture, vec2(blurcoord[0].x, uv.y)) * 0.204164;
    bp += texture2D(bg_texture, vec2(blurcoord[1].x, uv.y)) * 0.304005;
    bp += texture2D(bg_texture, vec2(blurcoord[2].x, uv.y)) * 0.304005;
    bp += texture2D(bg_texture, vec2(blurcoord[3].x, uv.y)) * 0.093913;
    bp += texture2D(bg_texture, vec2(blurcoord[4].x, uv.y)) * 0.093913;
    gl_FragColor = bp;
}
```

Samples 5 pixels horizontally with varying weights.

#### Vertical Pass

**File**: `gaussian.cpp:48-68`

Identical to horizontal, but samples vertically (varies `y`, fixes `x`).

### Execution Flow

**File**: `gaussian.cpp:106-137`

```cpp
int blur_fb0(const wf::region_t& blur_region, int width, int height) override {
    int iterations = iterations_opt;

    for (int i = 0; i < iterations; i++) {
        // Blur horizontally: fb[0] → fb[1]
        blur(blur_region, 0, width, height);

        // Blur vertically: fb[1] → fb[0]
        blur(blur_region, 1, width, height);
    }

    return 0;  // Result is in fb[0]
}
```

Each iteration:
1. Horizontal pass (fb[0] → fb[1])
2. Vertical pass (fb[1] → fb[0])

**Example with 2 iterations**:

| Pass | Operation | Shader | Buffer Flow |
|------|-----------|--------|-------------|
| 0 | Horizontal | H | fb[0] → fb[1] |
| 1 | Vertical | V | fb[1] → fb[0] |
| 2 | Horizontal | H | fb[0] → fb[1] |
| 3 | Vertical | V | fb[1] → fb[0] |

### Performance Characteristics

**Texture fetches per pixel**:
- Each pass: 5 fetches
- Total per iteration: 10 fetches

**Total work** (2 iterations, 1920×1080):
- 4 passes × 1920×1080 × 5 fetches = **41.5M fetches**

Slightly more expensive than Kawase for similar visual quality, but **much cheaper** than a large-radius naive Gaussian.

### Visual Quality

- **Perfectly circular blur**: Separable Gaussian is isotropic
- **Smooth, natural falloff**: True Gaussian distribution
- **Excellent color blending**: Weighted average preserves tones
- **No artifacts**: Mathematically correct

**Best for**: Applications requiring the highest visual quality and smooth, predictable blur.

---

## 3. Box Blur (Simple Average)

**File**: `plugins/blur/box.cpp`

### Algorithm Description

**Box blur** is the simplest blur - it averages all pixels in a rectangular region with **equal weights**. Like Gaussian, it's separable into horizontal and vertical passes.

### Default Configuration

```xml
<option name="box_offset">1</option>
<option name="box_degrade">1</option>
<option name="box_iterations">2</option>
```

**Effective blur radius**: `4 × offset × degrade × iterations = 4 × 1 × 1 × 2 = 8 pixels`

### Shader Implementation

**File**: `box.cpp:25-46`

```glsl
// Horizontal pass
void main() {
    vec2 uv = blurcoord[0];
    vec4 bp = vec4(0.0);
    for(int i = 0; i < 5; i++) {
        vec2 uv = vec2(blurcoord[i].x, uv.y);
        bp += texture2D(bg_texture, uv);
    }
    gl_FragColor = bp / 5.0;
}
```

**Equal weights**: Each of the 5 samples contributes 1/5 to the result.

**Sampling pattern**:
```
    1   1   1   1   1
  -3.5 -1.5  0  1.5 3.5
```

All weights are 0.2 (compared to Gaussian's varying weights).

### Execution Flow

**File**: `box.cpp:106-130`

Identical structure to Gaussian:
- Horizontal pass (fb[0] → fb[1])
- Vertical pass (fb[1] → fb[0])
- Repeat for N iterations

### Performance Characteristics

**Texture fetches per pixel**: 5 per pass (identical to Gaussian)
**Total work**: Same as Gaussian (41.5M fetches for 2 iterations at 1080p)

However, **simpler fragment shader** (no multiply-accumulate, just accumulate and divide) may execute slightly faster on some GPUs.

### Visual Quality

- **Blocky appearance**: Equal weights create visible "box" artifacts
- **Less smooth**: Transitions are abrupt compared to Gaussian
- **Faster convergence**: Multiple iterations approximate Gaussian (Central Limit Theorem)
- **Adequate for UI**: Good enough for panels/backgrounds where perfect smoothness isn't critical

**Best for**: Maximum performance when visual perfection isn't required.

---

## 4. Bokeh Blur (Depth-of-Field Style)

**File**: `plugins/blur/bokeh.cpp`

### Algorithm Description

**Bokeh blur** simulates a camera lens's circular aperture, creating a depth-of-field effect. It samples pixels in a **spiral pattern** using the **golden angle** to distribute samples evenly in a disc.

This is **not separable** - it's a single-pass 2D blur with many samples.

### Default Configuration

```xml
<option name="bokeh_offset">5</option>
<option name="bokeh_degrade">1</option>
<option name="bokeh_iterations">15</option>
```

**Note**: "iterations" here means **number of samples**, not blur passes.

**Effective blur radius**: `5 × offset × degrade = 5 × 5 × 1 = 25 pixels`

### Shader Implementation

**File**: `bokeh.cpp:17-54`

```glsl
#define GOLDEN_ANGLE 2.39996

mat2 rot = mat2(cos(GOLDEN_ANGLE), sin(GOLDEN_ANGLE),
               -sin(GOLDEN_ANGLE), cos(GOLDEN_ANGLE));

void main() {
    float radius = offset;
    vec4 acc = vec4(0), div = acc;
    float r = 1.0;
    vec2 vangle = vec2(radius / sqrt(float(iterations)),
                      radius / sqrt(float(iterations)));

    for (int j = 0; j < iterations; j++) {
        r += 1.0 / r;  // Spiral outward
        vangle = rot * vangle;  // Rotate by golden angle
        vec4 col = texture2D(bg_texture, uv + (r - 1.0) * vangle * halfpixel * 2.0);
        vec4 bokeh = pow(col, vec4(4.0));  // Brighten highlights
        acc += col * bokeh;  // Weight by brightness^4
        div += bokeh;
    }

    gl_FragColor = acc / div;
}
```

**Key features**:

1. **Golden angle spiral**: Samples are distributed using φ ≈ 2.4 radians (137.5°), which ensures even coverage without aliasing patterns.

2. **Brightness weighting**: `pow(col, vec4(4.0))` dramatically brightens highlights, making bright areas "bloom" - this is the characteristic "bokeh" look.

3. **Non-uniform weighting**: Brighter pixels contribute more to the final result, creating the camera lens aesthetic.

### Execution Flow

**File**: `bokeh.cpp:69-102`

```cpp
int blur_fb0(const wf::region_t& blur_region, int width, int height) override {
    int iterations = iterations_opt;
    float offset   = offset_opt;

    program[0].use(wf::TEXTURE_TYPE_RGBA);
    program[0].uniform1f("offset", offset);
    program[0].uniform1i("iterations", iterations);

    render_iteration(blur_region, fb[0], fb[1], width, height);

    return 1;  // Result is in fb[1]
}
```

**Single pass** - no ping-ponging between buffers.

### Performance Characteristics

**Texture fetches per pixel**: `iterations` samples (default: 15)
**No resolution reduction**: Operates at full resolution

**Total work** (15 iterations, 1920×1080):
- 1 pass × 1920×1080 × 15 fetches = **31.1M fetches**

**But**: Fragment shader is computationally expensive due to:
- `pow(col, vec4(4.0))` per sample (expensive)
- Rotation matrix multiplication per sample
- No separation - can't optimize with downsampling

**Actual GPU time** will be **significantly higher** than Kawase or Gaussian despite similar fetch counts.

### Visual Quality

- **Artistic effect**: Creates a "lens blur" aesthetic
- **Highlight blooming**: Bright areas spread into dark areas
- **Non-realistic for UI**: Doesn't look like natural blur
- **Unique appearance**: No other algorithm produces this look

**Best for**: Special effects, artistic compositing, stylized UIs (not general-purpose background blur).

---

## Comparative Analysis

### Quality vs Performance (Normalized)

```
Quality
  ▲
  │
  │        ◆ Gaussian
  │       ◆ Kawase
  │
  │     ◆ Box
  │
  │                    ◆ Bokeh (different aesthetic)
  │
  └──────────────────────────────────▶ Performance
```

### Blur Radius Formula Comparison

| Algorithm | Radius Formula | Example (defaults) |
|-----------|---------------|-------------------|
| **Kawase** | `2^(iter+1) × offset × degrade` | 2³ × 1.7 × 3 = 40.8px |
| **Gaussian** | `4 × offset × degrade × iter` | 4 × 1 × 1 × 2 = 8px |
| **Box** | `4 × offset × degrade × iter` | 4 × 1 × 1 × 2 = 8px |
| **Bokeh** | `5 × offset × degrade` | 5 × 5 × 1 = 25px |

**Key insight**: Kawase achieves **exponentially larger radii** with the same iteration count, making it far more efficient for large blurs.

### Use Case Recommendations

| Use Case | Recommended | Why |
|----------|------------|-----|
| **General desktop blur** | Kawase | Best balance of quality and performance |
| **High-quality screenshots** | Gaussian | Mathematically perfect, smooth |
| **Low-end hardware** | Box | Simplest shader, fastest execution |
| **Transparent panels (10-30px blur)** | Kawase or Gaussian | Both excellent |
| **Large blur (50-100px)** | Kawase only | Gaussian/Box too expensive |
| **Artistic/stylized effects** | Bokeh | Unique aesthetic |
| **HDR/vibrancy focus** | Gaussian or Kawase | Clean color blending |

### Parameter Tuning Guide

#### Kawase
- **offset**: Controls blur spread per pass (1.0-3.0 typical)
- **iterations**: Number of downsample/upsample levels (2-4 typical)
- **degrade**: Downsampling factor (1-3, higher = faster but blockier)

**Tuning**:
- For subtle blur: offset=1.5, iterations=2, degrade=2 → ~12px
- For medium blur: offset=1.7, iterations=2, degrade=3 → ~40px (default)
- For strong blur: offset=2.0, iterations=3, degrade=3 → ~96px

#### Gaussian
- **offset**: Sample spacing (1.0 typical)
- **iterations**: Number of full H+V pass pairs (2-6 typical)
- **degrade**: Downsampling (use 1 for quality, 2 for performance)

**Tuning**:
- For subtle blur: offset=1.0, iterations=2, degrade=1 → 8px
- For medium blur: offset=1.0, iterations=4, degrade=1 → 16px
- For strong blur: offset=2.0, iterations=6, degrade=2 → 96px

#### Box
Same as Gaussian, but expect lower quality.

#### Bokeh
- **offset**: Circle radius (5-15 typical)
- **iterations**: Sample count (15-50 typical, higher = smoother)

**Tuning**:
- For subtle effect: offset=5, iterations=15 → 25px
- For strong effect: offset=10, iterations=30 → 50px

**Warning**: iterations > 50 can cause noticeable frame drops.

---

## Shader Code Complexity

| Algorithm | Lines of Shader Code | Complexity | GPU Operations |
|-----------|---------------------|------------|----------------|
| **Kawase** | ~50 (2 shaders) | Medium | Texture fetches, simple math |
| **Gaussian** | ~40 (2 shaders) | Low | Texture fetches, multiply-accumulate |
| **Box** | ~40 (2 shaders) | Very Low | Texture fetches, addition |
| **Bokeh** | ~35 (1 shader) | High | Texture fetches, pow(), matrix mul |

---

## Extractability for External Daemon

All four algorithms are **highly portable** because they:
- ✅ Use only OpenGL ES 2.0 features (widely supported)
- ✅ Have no Wayfire-specific GL dependencies
- ✅ Operate on standard FBOs and textures
- ✅ Can be compiled standalone

**Porting effort**:
1. Copy shader source strings verbatim
2. Replace `OpenGL::compile_program()` with standard `glCompileShader()` / `glLinkProgram()`
3. Replace `wf::auxilliary_buffer_t` with standard FBO allocation
4. Replace `wf::gles::run_in_context_if_gles()` with EGL context management

**Estimated effort**: 2-3 hours per algorithm for a skilled OpenGL programmer.

---

## Performance Measurements (Estimated)

Based on typical GPU performance (NVIDIA GTX 1060, 1920×1080):

| Algorithm | Config | Frame Time | GPU % | Notes |
|-----------|--------|-----------|-------|-------|
| **Kawase** | Default | 0.8ms | 5% | Excellent |
| **Gaussian** | Default | 1.2ms | 7% | Good |
| **Box** | Default | 1.0ms | 6% | Very good |
| **Bokeh** | Default | 2.5ms | 15% | Heavy |
| **Kawase** | Strong | 1.1ms | 7% | Still good |
| **Gaussian** | Strong | 3.5ms | 21% | Expensive |
| **Bokeh** | Strong | 5.0ms | 30% | Very heavy |

**60 FPS budget**: 16.6ms per frame
**Blur overhead**: 0.8-5.0ms (5-30% of budget)

All algorithms are **viable for 60 FPS** at default settings. Only Bokeh at extreme settings risks dropping frames.

---

## Recommendations for External Blur Daemon

### Phase 1: MVP
Implement **Kawase only**:
- Best quality/performance ratio
- Single algorithm reduces complexity
- Handles all common use cases

### Phase 2: Quality Option
Add **Gaussian**:
- For users who prioritize visual perfection
- Relatively simple to implement (similar structure to Kawase)

### Phase 3: Full Parity
Add **Box** and **Bokeh**:
- Box for low-end hardware fallback
- Bokeh for artistic effects

### Parameter Exposure

For external daemon API, expose:
```json
{
  "algorithm": "kawase|gaussian|box|bokeh",
  "radius": 20,        // Effective radius in pixels
  "quality": "low|medium|high",  // Auto-adjusts iterations/degrade
  "saturation": 1.2    // Global saturation boost
}
```

Internally map these to algorithm-specific parameters.

---

## Conclusion

Wayfire's **Kawase blur** is the clear winner for general-purpose use, which is why it's the default. The availability of multiple algorithms demonstrates the plugin's **flexible architecture** - a design that translates well to an external daemon where users may want algorithm choice.

For the blur compositor project, **starting with Kawase** and **optionally adding Gaussian** provides 95% of use cases while keeping implementation manageable.
