# Quick Start Guide

Get wlblur running in 5 minutes.

## Step 1: Install wlblur

See [Installation Guide](Installation) for build instructions.

## Step 2: Create Configuration File

```bash
mkdir -p ~/.config/wlblur
cat > ~/.config/wlblur/config.toml << 'EOF'
[daemon]
socket_path = "/run/user/1000/wlblur.sock"
log_level = "info"

[defaults]
algorithm = "kawase"
radius = 5.0
passes = 3
saturation = 1.1

[presets.window]
radius = 8.0
passes = 3
saturation = 1.15

[presets.panel]
radius = 4.0
passes = 2
brightness = 1.05

[presets.hud]
radius = 12.0
passes = 4
vibrancy = 0.2
saturation = 1.2

[presets.tooltip]
radius = 2.0
passes = 1
EOF
```

## Step 3: Start the Daemon

```bash
wlblurd &
```

You should see:
```
[wlblurd] Listening on /run/user/1000/wlblur.sock
[wlblurd] Configuration loaded: 4 presets
```

## Step 4: Configure Your Compositor

*(Requires compositor integration - currently in development)*

**ScrollWM** (when available):
```toml
[blur]
enabled = true
daemon_socket = "/run/user/1000/wlblur.sock"
```

## Step 5: Test with Example

```bash
# Test blur on PNG image
./build/examples/blur-png input.png output.png

# Test IPC connection
./build/examples/ipc-client-example
```

## Troubleshooting

**Daemon won't start:**
- Check dependencies: `ldd build/wlblurd/wlblurd`
- Check logs: `wlblurd --log-level debug`

**Config not loading:**
- Verify path: `ls ~/.config/wlblur/config.toml`
- Test config: `wlblurd --config ~/.config/wlblur/config.toml`

See [Troubleshooting Guide](../User-Guide/Troubleshooting) for more help.

## Next Steps

- [Configuration Guide](../User-Guide/Configuration) - Customize blur settings
- [Presets Guide](../User-Guide/Presets) - Create custom presets
- [Hot Reload](../User-Guide/Hot-Reload) - Live config updates
