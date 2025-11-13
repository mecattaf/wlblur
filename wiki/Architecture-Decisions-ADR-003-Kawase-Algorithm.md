# ADR-003: Dual Kawase Blur Algorithm Selection

**Status**: Proposed
**Date**: 2025-01-15

## Context

Our blur daemon needs to implement a blur algorithm that provides high visual quality while maintaining performance suitable for real-time rendering at 60 FPS. The algorithm must:

1. **High quality**: Apple-level frosted glass aesthetic
2. **Fast performance**: <1.5ms for 1920×1080 on mid-range GPU
3. **Proven in production**: Used by existing Wayland compositors
4. **Well-documented**: Clear implementation references available
5. **Extensible**: Foundation for future enhancements (vibrancy, tint)

### Background: Blur Algorithms in Wayland Compositors

From our investigation of Hyprland, Wayfire, and SceneFX, we identified several blur algorithms in production use:

**Hyprland:** Dual Kawase (1-8 passes, default 1)
- Performance: 0.8-1.5ms uncached, 0.05ms cached
- Quality: High (Apple-level with vibrancy)
- Samples per pixel: 16-18

**Wayfire:** Multiple algorithms
- Kawase: 0.8ms, high quality, 16-18 samples
- Gaussian: 1.2ms, highest quality, 25-49 samples
- Box: 0.4ms, lower quality, 9 samples
- Bokeh: 2-5ms, artistic quality, variable samples

**SceneFX:** Dual Kawase (fixed 3 passes)
- Performance: 1.2ms
- Quality: High
- Samples per pixel: 16-18

## Decision

**We will implement Dual Kawase blur as the default algorithm for the MVP, with extensibility for additional algorithms in future phases.**

### What is Kawase Blur?

Kawase blur is a multi-pass approximation of Gaussian blur that achieves similar visual quality with significantly fewer texture samples. Developed by Masaki Kawase at FromSoftware for real-time game rendering.

**Key Properties:**
- **Separable approximation**: Samples along diagonals instead of horizontal/vertical
- **Progressive refinement**: Each pass doubles effective blur radius
- **Exponential coverage**: N passes = 2^N coverage radius
- **Low sample count**: Only 5-8 samples per pass (vs 9-49 for Gaussian)

### Dual Kawase Variant

"Dual" Kawase uses two different kernels:

**Downsample Pass (blur1.frag):**
- 5-sample kernel: center + 4 diagonal corners
- Reduces resolution by 2×
- Expands blur radius

**Upsample Pass (blur2.frag):**
- 8-sample kernel: 4 cardinal + 4 diagonal directions
- Restores resolution by 2×
- Adds weighted blending for smooth result

**Pipeline:**
```
Original (1920×1080)
  ↓ blur1 (downsample)
Half (960×540)
  ↓ blur1 (downsample)
Quarter (480×270)
  ↓ blur2 (upsample)
Half (960×540)
  ↓ blur2 (upsample)
Original (1920×1080) - Blurred
```

### Shader Implementation

**Downsample (5 samples):**
```glsl
// blur1.frag
vec4 kawase_downsample(sampler2D tex, vec2 uv, vec2 halfpixel) {
    vec4 sum = texture(tex, uv) * 4.0;          // Center (weight 4)
    sum += texture(tex, uv - halfpixel.xy);     // Top-left
    sum += texture(tex, uv + halfpixel.xy);     // Bottom-right
    sum += texture(tex, uv + vec2(halfpixel.x, -halfpixel.y));  // Top-right
    sum += texture(tex, uv - vec2(halfpixel.x, -halfpixel.y));  // Bottom-left
    return sum / 8.0;
}
```

**Upsample (8 samples):**
```glsl
// blur2.frag
vec4 kawase_upsample(sampler2D tex, vec2 uv, vec2 halfpixel) {
    vec4 sum = vec4(0.0);
    sum += texture(tex, uv + vec2(-halfpixel.x * 2, 0.0));  // Left
    sum += texture(tex, uv + vec2(-halfpixel.x, halfpixel.y)) * 2.0;  // Top-left
    sum += texture(tex, uv + vec2(0.0, halfpixel.y * 2));   // Top
    sum += texture(tex, uv + vec2(halfpixel.x, halfpixel.y)) * 2.0;   // Top-right
    sum += texture(tex, uv + vec2(halfpixel.x * 2, 0.0));   // Right
    sum += texture(tex, uv + vec2(halfpixel.x, -halfpixel.y)) * 2.0;  // Bottom-right
    sum += texture(tex, uv + vec2(0.0, -halfpixel.y * 2));  // Bottom
    sum += texture(tex, uv + vec2(-halfpixel.x, -halfpixel.y)) * 2.0; // Bottom-left
    return sum / 12.0;
}
```

## Alternatives Considered

### Alternative 1: Gaussian Blur (Separable)

**Approach:** Classic Gaussian kernel with separate horizontal and vertical passes.

**Pros:**
- Mathematically "correct" blur (matches true Gaussian function)
- Highest quality (smoothest results)
- Well-understood algorithm
- Available in Wayfire

**Cons:**
- **More samples**: 25-49 samples per pixel (vs 16-18 for Kawase)
- **Slower**: 1.2ms vs 0.8ms for Kawase (50% slower)
- **Separable passes still required**: Not simpler to implement
- **Diminishing returns**: Visual difference from Kawase is subtle

**Performance Comparison (1920×1080, Wayfire data):**
- Gaussian 9-tap: 0.9ms
- Gaussian 13-tap: 1.2ms
- Gaussian 25-tap: 2.1ms
- Kawase 3 passes: 0.8ms ✅

**Why Deferred:** Kawase provides 95% of the quality at 66% of the cost. Add Gaussian later as "high quality" option for users who prefer it and can spare the GPU time.

### Alternative 2: Box Blur

**Approach:** Simple average of all pixels in a square region.

**Pros:**
- Fastest algorithm (0.4ms)
- Simplest implementation
- Predictable results

**Cons:**
- **Low quality**: Visible block artifacts
- **Harsh edges**: Not smooth like Gaussian
- **Doesn't match Apple aesthetic**: Too crude
- **Not used in premium compositors**: Hyprland and SceneFX don't offer it

**Why Rejected for MVP:** Quality too low for target aesthetic. Could add later as "fast" option for low-end hardware.

### Alternative 3: Bokeh Blur

**Approach:** Artistic blur with visible bokeh circles, simulating camera lens out-of-focus.

**Pros:**
- Unique aesthetic (different from other compositors)
- Artistic look
- Good for photography apps

**Cons:**
- **Very slow**: 2-5ms (2.5-6× slower than Kawase)
- **Not suitable for UI**: Too artistic, distracting
- **Wrong aesthetic**: Doesn't match macOS frosted glass
- **Complex implementation**: Golden angle spiral sampling

**Why Rejected:** Wrong aesthetic for UI blur. Could be useful for special effects layer, but not as default.

### Alternative 4: Custom Optimized Kernel

**Approach:** Design our own blur kernel optimized specifically for UI backgrounds.

**Pros:**
- Could potentially be faster
- Optimized for our exact use case
- Unique to our project

**Cons:**
- **Unproven quality**: Would need extensive testing
- **No reference implementations**: Starting from scratch
- **High risk**: Might not be better than Kawase
- **Delays MVP**: Research phase would add weeks
- **Harder to tune**: No community knowledge

**Why Rejected:** Too risky for MVP. Kawase is proven. If performance becomes critical, optimize Kawase implementation first.

### Alternative 5: Compute Shader Blur

**Approach:** Use compute shaders (OpenGL 4.3+) instead of fragment shaders for potentially better performance.

**Pros:**
- Can be faster (shared memory, better parallelism)
- More control over GPU execution
- Modern approach

**Cons:**
- **Requires OpenGL 4.3+**: Not available on all systems
- **More complex**: Harder to implement correctly
- **Not significantly faster**: Fragment shaders are already fast
- **Less portable**: GLES 3.0 doesn't have compute shaders

**Why Deferred:** Fragment shader Kawase is already fast enough (<1.5ms target). Add compute shader path later as optimization if needed.

## Consequences

### Positive

1. **Proven Quality**: Used in production by Hyprland, Wayfire, SceneFX
   - Users already familiar with visual quality
   - Known to match Apple aesthetic (with vibrancy enhancement)

2. **Excellent Performance**: 0.8-1.2ms for 1920×1080
   - Well within 1.5ms target
   - Fast enough for 60 FPS with headroom
   - 4K: ~3-4ms (scales linearly with pixel count)

3. **Code Reuse**: ~600 lines available from Wayfire
   - Clean implementation in `plugins/blur/kawase.cpp`
   - Portable GLSL ES 2.0 shaders
   - Proven multi-pass framework

4. **Low Sample Count**: 16-18 samples per pixel total
   - vs 25-49 for Gaussian
   - Lower memory bandwidth
   - Better for integrated GPUs

5. **Configurable Quality**: Passes can be adjusted
   - 1 pass (Hyprland default): Fast, light blur
   - 3 passes (SceneFX): Balanced quality/speed
   - 5+ passes: Heavy blur for special effects

6. **Foundation for Vibrancy**: Works well with post-processing
   - Hyprland adds vibrancy on top of Kawase
   - HSL saturation boost enhances blurred colors
   - Path to Apple-level material system

7. **GPU Vendor Agnostic**: Works on all GPUs
   - Intel, AMD, NVIDIA tested in Wayfire/Hyprland
   - No special hardware features required
   - GLSL ES 2.0 (2007 standard)

### Negative

1. **Not "Mathematically Correct"**: Approximation of Gaussian
   - Subtle differences from true Gaussian
   - Mitigation: Visual quality is what matters, and Kawase looks great

2. **Requires Multiple Passes**: Not single-pass algorithm
   - Need ping-pong framebuffers
   - More GL state management
   - Mitigation: All quality blurs are multi-pass, this is expected

3. **Quality Depends on Pass Count**: Users might want more
   - 1 pass = subtle blur
   - 3 passes = good blur
   - 8 passes = extreme blur
   - Mitigation: Make passes configurable (default 3)

4. **Not Unique to Our Project**: Other compositors use it
   - Might not differentiate us
   - Mitigation: Vibrancy and material system will differentiate

## Implementation Plan

### MVP (Phase 1): Basic Kawase

**Deliverables:**
- Port Kawase shaders from Wayfire
- Implement 3-pass downsample/upsample pipeline
- FBO ping-pong management
- Configurable pass count

**Timeline:** Week 4-5

**Code Estimate:** ~400 lines (ported from Wayfire)

### Enhancement (Phase 2): Vibrancy

**Deliverables:**
- HSL color space conversion (from Hyprland)
- Saturation boost in upsample pass
- Configurable vibrancy strength

**Timeline:** Week 7

**Code Estimate:** ~150 lines

### Extension (Phase 3): Additional Algorithms

**Deliverables:**
- Gaussian blur (from Wayfire)
- Box blur (from Wayfire)
- Algorithm selection API

**Timeline:** Week 10

**Code Estimate:** ~300 lines

### Optimization (Phase 4): Advanced Techniques

**Deliverables:**
- Compute shader variant (OpenGL 4.3+)
- Half-float textures (less bandwidth)
- Shader compilation caching

**Timeline:** Post-MVP

**Code Estimate:** ~200 lines

## Performance Targets

**Target GPU:** Mid-range (Intel UHD 620, AMD Vega 7, NVIDIA GTX 1650)

**Resolution: 1920×1080**
- 1 pass: <0.5ms
- 3 passes: <1.2ms ✅ (default)
- 5 passes: <2.0ms

**Resolution: 3840×2160 (4K)**
- 3 passes: <4.0ms ✅
- Scales linearly with pixel count (4× pixels = 4× time)

**With IPC overhead:**
- Total: 1.2ms + 0.2ms = 1.4ms ✅ (within 1.5ms target)

## Blur Radius Formula

**Critical Formula from SceneFX:**
```c
blur_size = 2^(num_passes + 1) × radius_pixels
```

**Examples:**
- 3 passes, radius=5: 2^4 × 5 = 80 pixels
- 5 passes, radius=5: 2^6 × 5 = 320 pixels

**Usage:**
- Damage region expansion: Must expand by blur_size
- Visibility calculations: Blur affects pixels within blur_size radius
- Artifact prevention: Save/restore padding at blur_size edge

## Quality Validation

**Comparison to macOS:**
- macOS Big Sur+ uses similar multi-pass blur
- With vibrancy enhancement, visual quality matches
- Validated by Hyprland users: "looks like macOS"

**Comparison to Windows 11:**
- Windows 11 Acrylic uses separable Gaussian
- Kawase + vibrancy is comparable quality
- Actually more performant (Windows Acrylic can lag on older hardware)

## References

- Investigation docs:
  - [SceneFX Investigation Summary](Technical-Investigation-SceneFX-Summary) - Dual Kawase implementation details
  - [Wayfire Algorithm Analysis](Technical-Investigation-Wayfire-Algorithm-Analysis) - Algorithm comparison
  - [Comparative Analysis](Technical-Investigation-Comparative-Analysis) - Performance characteristics

- External resources:
  - [ARM SIGGRAPH 2015 - Bandwidth Efficient Rendering](https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/siggraph2015_2D00_mmg_2D00_marius_2D00_notes.pdf) - Original Kawase presentation
  - [Intel Efficient Blur](https://www.intel.com/content/www/us/en/developer/articles/technical/an-investigation-of-fast-real-time-gpu-based-image-blur-algorithms.html) - Kawase vs alternatives

- Code references:
  - Wayfire: `plugins/blur/kawase.cpp` (lines 25-120)
  - Hyprland: `src/render/OpenGL.cpp` (blur implementation)
  - SceneFX: `render/fx_renderer/gles3/shaders/blur1.frag`, `blur2.frag`

- Related ADRs:
  - [ADR-005: SceneFX extraction](ADR-005-SceneFX-Extraction) (Kawase shader porting)

## Community Feedback

We invite feedback on this decision:

- **Artists/Designers**: Does Kawase blur match your aesthetic expectations for frosted glass?
- **Performance testers**: Can you validate <1.2ms on your GPU?
- **Algorithm experts**: Are there newer blur algorithms we should consider for Phase 2+?
- **Users**: Would you prefer more algorithm options (Gaussian, Box) or just well-tuned Kawase?

Please open issues at [project repository] or discuss in [community forum].
