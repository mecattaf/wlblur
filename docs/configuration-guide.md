# wlblur Configuration Guide

**Version:** 1.0 (for wlblur v1.0+)
**Last Updated:** 2025-11-13
**Audience:** End users

---

## Table of Contents

1. [Quick Start](#quick-start)
2. [Configuration File Location](#configuration-file-location)
3. [Basic Configuration](#basic-configuration)
4. [Understanding Presets](#understanding-presets)
5. [Customizing Blur Parameters](#customizing-blur-parameters)
6. [Hot Reload](#hot-reload)
7. [Troubleshooting](#troubleshooting)
8. [Advanced Topics](#advanced-topics)

---

## Quick Start

### Do I Need to Configure wlblur?

**No!** wlblur works out-of-the-box with sensible defaults. Configuration is optional and only needed if you want to customize the blur appearance.

###First Time Setup

1. **Install wlblur** (see README.md)
2. **Start the daemon**: `wlblurd &`
3. **Enable blur in your compositor** (see compositor-specific docs)
4. **Done!** Blur works with default settings

### When to Configure

Configure wlblur if you want to:
- Adjust blur strength (more or less intense)
- Change blur behavior for different surface types (panels vs windows)
- Experiment with different visual styles
- Fine-tune performance on your hardware

---

## Configuration File Location

wlblur looks for configuration in the following locations (in order):

1. `$XDG_CONFIG_HOME/wlblur/config.toml`
2. `~/.config/wlblur/config.toml`
3. `/etc/wlblur/config.toml` (system-wide)
4. Built-in defaults (if no file found)

**Recommended location:** `~/.config/wlblur/config.toml`

### Creating Your Configuration

```bash
# Create config directory
mkdir -p ~/.config/wlblur

# Copy example config
cp /usr/share/doc/wlblur/examples/wlblur-config.toml ~/.config/wlblur/config.toml

# Or create from scratch
cat > ~/.config/wlblur/config.toml << 'EOF'
[defaults]
algorithm = "kawase"
num_passes = 3
radius = 5.0
saturation = 1.1

[presets.window]
radius = 8.0
passes = 3
saturation = 1.15
EOF
```

---

## Basic Configuration

### Minimal Configuration

The simplest configuration just adjusts the default blur strength:

```toml
# ~/.config/wlblur/config.toml

[defaults]
radius = 8.0          # Stronger blur
num_passes = 4        # Smoother blur
saturation = 1.2      # More saturated colors
```

**This affects all blur unless overridden by presets.**

### Recommended Starting Point

Copy this minimal config and adjust to taste:

```toml
[defaults]
algorithm = "kawase"
num_passes = 3
radius = 5.0
saturation = 1.1
noise = 0.02

[presets.window]
radius = 8.0
num_passes = 3

[presets.panel]
radius = 4.0
num_passes = 2
brightness = 1.05
```

---

## Understanding Presets

### What Are Presets?

**Presets** are named collections of blur parameters. Your compositor references presets by name (e.g., "panel", "window") instead of specifying all parameters.

**Benefits:**
- **Consistency**: Panels look the same in all compositors
- **Convenience**: Change blur once, applies everywhere
- **Future-proof**: New features work automatically

### Standard Presets

wlblur defines four standard presets:

| Preset | Use Case | Default Settings |
|--------|----------|------------------|
| **window** | Application windows | Moderate blur (radius=8.0, passes=3) |
| **panel** | Desktop panels (waybar, quickshell) | Light blur (radius=4.0, passes=2) |
| **hud** | Popups, launchers, notifications | Strong blur (radius=12.0, passes=4) |
| **tooltip** | Tooltips, small popups | Minimal blur (radius=2.0, passes=1) |

### How Compositors Use Presets

Your compositor automatically maps surface types to preset names:

```
┌─────────────────┐
│  Application    │  → Compositor detects type
│  (quickshell)   │
└─────────────────┘
         ↓
┌─────────────────┐
│  Compositor     │  → Maps to preset name
│  "this is a     │     (layershell → "panel")
│   panel"        │
└─────────────────┘
         ↓
┌─────────────────┐
│  wlblur         │  → Resolves preset to
│  preset="panel" │     parameters
└─────────────────┘
```

**You don't configure the mapping** - the compositor does this automatically.

### Customizing Presets

Override standard presets to change their behavior:

```toml
# Make all panels have stronger blur
[presets.panel]
radius = 6.0          # Stronger than default 4.0
num_passes = 3        # More passes than default 2
saturation = 1.15     # More saturated
```

### Creating Custom Presets

Define your own presets for special cases:

```toml
# Custom preset for terminal windows
[presets.terminal]
radius = 3.0          # Less blur to keep text readable
num_passes = 2
saturation = 1.0      # No saturation boost

# Custom preset for floating windows
[presets.floating]
radius = 10.0
num_passes = 4
vibrancy = 0.2
```

**Note:** You need to configure your compositor to use custom presets. See compositor-specific docs.

---

## Customizing Blur Parameters

### Parameter Reference

| Parameter | Range | Default | Effect |
|-----------|-------|---------|--------|
| `algorithm` | "kawase" | "kawase" | Blur algorithm (only kawase in v1.0) |
| `num_passes` | 1-8 | 3 | Blur smoothness (more = smoother, slower) |
| `radius` | 1.0-20.0 | 5.0 | Blur strength (higher = more blur) |
| `brightness` | 0.0-2.0 | 1.0 | Brightness adjustment |
| `contrast` | 0.0-2.0 | 1.0 | Contrast adjustment |
| `saturation` | 0.0-2.0 | 1.1 | Saturation adjustment |
| `noise` | 0.0-1.0 | 0.02 | Noise overlay (reduces banding) |
| `vibrancy` | 0.0-2.0 | 0.0 | HSL saturation boost |

### radius - Blur Strength

**Effect:** Controls how far the blur spreads.

```toml
radius = 2.0    # Subtle, barely noticeable
radius = 5.0    # Moderate, balanced (default)
radius = 10.0   # Strong, prominent
radius = 20.0   # Maximum, very blurred
```

**Performance impact:** Higher radius = slower rendering

**Recommendation:**
- Panels: 3.0-5.0
- Windows: 6.0-10.0
- HUD/Popups: 10.0-15.0

### num_passes - Blur Smoothness

**Effect:** Controls blur quality. More passes = smoother, more Gaussian-like.

```toml
num_passes = 1    # Fast, slightly blocky
num_passes = 2    # Good, balanced
num_passes = 3    # Better, smooth (default)
num_passes = 4    # Best, very smooth
num_passes = 8    # Overkill, diminishing returns
```

**Performance impact:** Each pass doubles render time

**Recommendation:**
- Tooltips: 1
- Panels: 2-3
- Windows: 3-4
- HUD: 3-5

### saturation - Color Intensity

**Effect:** Boosts or reduces color saturation.

```toml
saturation = 0.0    # Grayscale
saturation = 0.8    # Desaturated
saturation = 1.0    # No change
saturation = 1.1    # Slightly boosted (default)
saturation = 1.3    # Vivid
saturation = 2.0    # Maximum, oversaturated
```

**Recommendation:** 1.1-1.2 for most cases. Higher values can look unnatural.

### brightness & contrast

**Effect:** Adjust blur brightness and contrast.

```toml
brightness = 0.8    # Darker
brightness = 1.0    # No change (default)
brightness = 1.2    # Brighter

contrast = 0.8      # Lower contrast
contrast = 1.0      # No change (default)
contrast = 1.2      # Higher contrast
```

**Use case:** Brighten dark panels, darken bright overlays.

### noise - Noise Overlay

**Effect:** Adds subtle noise to reduce color banding.

```toml
noise = 0.0      # No noise (may show banding)
noise = 0.02     # Subtle (default, recommended)
noise = 0.05     # Noticeable
noise = 0.1      # Grainy
```

**Recommendation:** Keep at 0.01-0.03 for best results.

### vibrancy - HSL Saturation Boost

**Effect:** Hyprland-style color boost (HSL saturation, not RGB).

```toml
vibrancy = 0.0     # No vibrancy (default)
vibrancy = 0.1     # Subtle boost
vibrancy = 0.2     # Moderate boost
vibrancy = 0.5     # Strong boost
vibrancy = 1.0     # Maximum (can look oversaturated)
```

**Difference from saturation:** Vibrancy preserves luminance better, looks more natural for high values.

**Recommendation:** Use vibrancy for HUD elements, keep windows at 0.0.

---

## Hot Reload

### What is Hot Reload?

**Hot reload** lets you change configuration without restarting the daemon or compositor. Changes apply immediately to all running compositors.

### How to Hot Reload

```bash
# 1. Edit your config
vim ~/.config/wlblur/config.toml

# 2. Reload daemon
killall -USR1 wlblurd

# 3. Changes apply immediately
# (No compositor restart needed!)
```

**Alternative methods:**

```bash
# Using pkill
pkill -USR1 wlblurd

# Using systemd (if running as service)
systemctl --user reload wlblurd
```

### Experimenting with Settings

Hot reload makes it easy to find your perfect settings:

```bash
# 1. Open two terminals

# Terminal 1: Watch logs
journalctl -f -u wlblurd

# Terminal 2: Edit and reload
vim ~/.config/wlblur/config.toml
# Change radius from 5.0 to 10.0
killall -USR1 wlblurd
# See changes immediately!

# Repeat until satisfied
```

### What Happens on Reload Error?

If your config has errors, the daemon:
1. **Logs the error** (check logs)
2. **Keeps the old config** (doesn't break)
3. **Continues running normally**

**Example error:**

```
[ERROR] Config reload failed: radius 25.0 exceeds maximum 20.0
[ERROR] Keeping old configuration
```

**Fix the error and reload again.**

---

## Troubleshooting

### Daemon Won't Start

**Symptom:** `wlblurd` exits immediately

**Causes:**
1. Socket already in use
2. Config parse error
3. Missing dependencies

**Solution:**

```bash
# Check logs
journalctl -u wlblurd

# Kill existing daemon
killall wlblurd

# Try starting in foreground
wlblurd --log-level=debug

# Check socket
ls -l /run/user/$(id -u)/wlblur.sock
```

### Config Not Loading

**Symptom:** Changes to config have no effect

**Checks:**

```bash
# 1. Verify config location
ls ~/.config/wlblur/config.toml

# 2. Check config syntax
tomlq . ~/.config/wlblur/config.toml

# 3. Check daemon is reading it
wlblurd --config ~/.config/wlblur/config.toml

# 4. Check logs for errors
journalctl -u wlblurd | grep ERROR
```

**Common mistakes:**
- Wrong file location (use `~/.config/wlblur/`)
- TOML syntax error (missing quotes, wrong brackets)
- Forgot to reload (`killall -USR1 wlblurd`)

### Preset Not Found

**Symptom:** Warning in logs: `Preset 'foo' not found`

**Cause:** Compositor requested preset that doesn't exist in config

**Solution:**

```toml
# Add the missing preset
[presets.foo]
radius = 5.0
num_passes = 3
```

**Or** check compositor config for typos:

```toml
# Compositor config
[blur]
preset = "panle"  # Typo! Should be "panel"
```

### Performance Issues

**Symptom:** Low FPS with blur enabled

**Solutions:**

1. **Reduce radius:**
   ```toml
   radius = 4.0  # Instead of 10.0
   ```

2. **Reduce passes:**
   ```toml
   num_passes = 2  # Instead of 4
   ```

3. **Check GPU usage:**
   ```bash
   # NVIDIA
   nvidia-smi

   # AMD
   radeontop
   ```

4. **Profile with gpuvis:**
   ```bash
   gpuvis ~/blur-profile.trace
   ```

### Blur Looks Wrong

**Symptom:** Blur doesn't look like expected

**Causes & Solutions:**

| Problem | Cause | Solution |
|---------|-------|----------|
| Too weak | Low radius | Increase `radius` |
| Too strong | High radius | Decrease `radius` |
| Blocky | Low passes | Increase `num_passes` |
| Slow | High passes | Decrease `num_passes` |
| Oversaturated | High saturation | Decrease `saturation` |
| Banding | No noise | Add `noise = 0.02` |
| Too dark | Low brightness | Increase `brightness` |

---

## Advanced Topics

### Algorithm Selection (v2.0+)

**Coming in v2.0:** Multiple blur algorithms

```toml
[presets.window]
algorithm = "kawase"      # Default, balanced

[presets.window_hq]
algorithm = "gaussian"    # Highest quality, slower

[presets.window_fast]
algorithm = "box"         # Fastest, lower quality

[presets.artistic]
algorithm = "bokeh"       # Artistic effect
```

**Current (v1.0):** Only `algorithm = "kawase"` is supported.

### Per-Compositor Overrides

Some compositors let you override daemon presets:

```toml
# Compositor config (scroll example)
[blur]
enabled = true

# Override daemon preset for specific app
[[blur.rules]]
app_id = "kitty"
preset = "terminal"  # Use custom preset
```

**See your compositor's documentation for details.**

### Material System (v2.0+)

**Coming in v2.0:** macOS-style material presets

```toml
[materials.sidebar]
base_preset = "panel"
appearance = "auto"  # light/dark/auto
adapt_to_wallpaper = true
```

**This will enable preset switching based on system theme.**

### Debugging Configuration

**Enable debug logging:**

```bash
# In config
[daemon]
log_level = "debug"

# Or command line
wlblurd --log-level=debug

# Watch logs
journalctl -f -u wlblurd
```

**Useful log messages:**

```
[DEBUG] Loaded preset 'panel': radius=4.0 passes=2
[DEBUG] Client 1 using preset 'panel'
[INFO] Configuration reloaded successfully (3 presets)
[WARN] Preset 'custom' not found, using defaults
[ERROR] Config validation failed: radius 25.0 exceeds maximum 20.0
```

---

## Example Configurations

### Minimal (Just the Essentials)

```toml
[defaults]
radius = 6.0
num_passes = 3
saturation = 1.1
```

### Recommended (Balanced)

```toml
[defaults]
algorithm = "kawase"
num_passes = 3
radius = 5.0
saturation = 1.1
noise = 0.02

[presets.window]
radius = 8.0
num_passes = 3

[presets.panel]
radius = 4.0
num_passes = 2
brightness = 1.05
```

### Performance-Focused (Fast)

```toml
[defaults]
num_passes = 2
radius = 4.0
saturation = 1.05
noise = 0.01

[presets.window]
radius = 6.0
num_passes = 2

[presets.panel]
radius = 3.0
num_passes = 1
```

### Quality-Focused (Slow but Beautiful)

```toml
[defaults]
num_passes = 5
radius = 8.0
saturation = 1.15
noise = 0.03
vibrancy = 0.05

[presets.window]
radius = 12.0
num_passes = 5
saturation = 1.2

[presets.panel]
radius = 6.0
num_passes = 3
brightness = 1.1
saturation = 1.15

[presets.hud]
radius = 16.0
num_passes = 6
vibrancy = 0.3
```

### macOS-Inspired

```toml
[defaults]
num_passes = 4
radius = 6.0
saturation = 1.12
brightness = 1.02
noise = 0.015

[presets.window]
radius = 8.0
num_passes = 4
saturation = 1.15
vibrancy = 0.05

[presets.panel]
radius = 5.0
num_passes = 3
brightness = 1.05
saturation = 1.12

[presets.hud]
radius = 12.0
num_passes = 5
saturation = 1.18
vibrancy = 0.15
```

---

## Further Reading

- **Architecture:** `docs/architecture/04-configuration-system.md` - Technical details
- **ADR-006:** `docs/decisions/006-daemon-configuration-with-presets.md` - Design rationale
- **Example Config:** `docs/examples/wlblur-config.toml` - Annotated example
- **Compositor Integration:** Check your compositor's documentation for blur configuration

---

**Need help?** Open an issue at https://github.com/[project]/issues
