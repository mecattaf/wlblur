# Hot Reload

## What Is Hot Reload?

Hot reload lets you modify blur configuration and apply changes instantly without restarting the daemon or compositor.

## How to Hot Reload

### Step 1: Edit Config
```bash
vim ~/.config/wlblur/config.toml

# Change a value, e.g.:
[presets.window]
radius = 10.0  # Was 8.0
```

### Step 2: Trigger Reload
```bash
killall -USR1 wlblurd
```

Or find PID:
```bash
PID=$(pgrep wlblurd)
kill -USR1 $PID
```

### Step 3: Verify
Check daemon logs:
```bash
[wlblurd] Received SIGUSR1, reloading configuration
[wlblurd] Configuration reloaded successfully
[wlblurd] Loaded 4 presets
```

## What Happens During Reload?

1. Daemon loads new config file
2. Validates all parameters
3. If valid: swaps to new config atomically
4. If invalid: keeps old config, logs error

**Your compositor never restarts!**

## Error Handling

If reload fails:
```bash
[wlblurd] Config validation failed: radius out of range
[wlblurd] Keeping old configuration
```

The daemon continues running with old config.

## Use Cases

### Experimentation
```bash
# Try different radius values
vim ~/.config/wlblur/config.toml  # radius = 5
killall -USR1 wlblurd
# See result

vim ~/.config/wlblur/config.toml  # radius = 10
killall -USR1 wlblurd
# Compare
```

### Per-Application Tuning
```bash
# Add new preset for specific app
[presets.firefox]
radius = 6.0
passes = 3

# Reload
killall -USR1 wlblurd

# Update compositor to use new preset
```

### Performance Tuning
```bash
# Reduce passes for better FPS
[defaults]
passes = 2  # Was 3

killall -USR1 wlblurd
```

## Limitations

- Only config file changes (not code changes)
- Applies to future blur requests (not retroactive)
- Unix-specific (SIGUSR1 signal)

## See Also

- [Configuration Guide](Configuration) - Parameter details
- [Presets](Presets) - Preset system
- [ADR-006: Daemon Configuration](../Architecture-Decisions/ADR-006-Daemon-Config) - Design rationale
