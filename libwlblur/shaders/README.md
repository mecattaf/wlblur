# wlblur Shaders

GLSL shaders for blur algorithms. These will be extracted from:
- SceneFX (Dual Kawase base implementation)
- Hyprland (vibrancy enhancements)
- Wayfire (additional algorithms)

## Expected Files
- kawase_downsample.frag.glsl - Downsample pass (5-tap)
- kawase_upsample.frag.glsl - Upsample pass (8-tap)
- blur_prepare.frag.glsl - Pre-processing
- blur_finish.frag.glsl - Post-processing effects
- vibrancy.frag.glsl - HSL color boost
- common.glsl - Shared functions

## Status
Awaiting extraction in task-2.
