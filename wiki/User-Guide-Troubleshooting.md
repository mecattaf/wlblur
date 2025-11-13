# Troubleshooting

## Daemon Issues

### Daemon won't start

**Symptom:** `wlblurd` exits immediately

**Check dependencies:**
```bash
ldd build/wlblurd/wlblurd

# Should show:
# libegl.so.1 => /lib/x86_64-linux-gnu/libegl.so.1
# libglesv2.so.2 => /lib/x86_64-linux-gnu/libglesv2.so.2
# libtoml.so => /lib/x86_64-linux-gnu/libtoml.so
```

**Check logs:**
```bash
wlblurd --log-level debug
```

**Common causes:**
- Missing dependencies: Install EGL, GLES, libdrm, tomlc99
- Socket path permission: Check `/run/user/$UID/` exists
- Already running: `killall wlblurd` first

### Config file not loading

**Symptom:** Daemon uses defaults despite config file

**Verify config exists:**
```bash
ls -la ~/.config/wlblur/config.toml
```

**Test manual path:**
```bash
wlblurd --config ~/.config/wlblur/config.toml --log-level debug
```

**Check TOML syntax:**
```bash
# Use online validator or:
python3 -c "import toml; toml.load(open('$HOME/.config/wlblur/config.toml'))"
```

**Common causes:**
- Config in wrong location
- TOML syntax error
- Invalid parameter values

### Hot reload fails

**Symptom:** `killall -USR1 wlblurd` has no effect

**Check daemon is running:**
```bash
pgrep wlblurd
```

**Check logs:**
```bash
journalctl -f | grep wlblurd
# Or if running in terminal, watch stdout
```

**Common causes:**
- Daemon not running
- Config has syntax errors
- Parameter values out of range

## Performance Issues

### Blur is laggy

**Reduce blur quality:**
```toml
[defaults]
passes = 2      # Instead of 3
radius = 4.0    # Instead of 8.0
```

**Check GPU usage:**
```bash
nvidia-smi  # NVIDIA
radeontop   # AMD
intel_gpu_top  # Intel
```

**Profile with logs:**
```bash
wlblurd --log-level debug
# Watch for timing logs
```

### High memory usage

**Check node count:**
```bash
# In logs, look for:
# [wlblurd] Active nodes: X
```

**Typical memory:**
- Base: ~16MB
- Per node: ~2-4MB
- 10 nodes: ~40-60MB total

**If excessive:** Check for node leaks (compositor not cleaning up)

## Integration Issues

### Compositor can't connect

**Check socket:**
```bash
ls -la /run/user/$UID/wlblur.sock

# Should show:
# srwx------ 1 user user 0 Jan 15 10:00 /run/user/1000/wlblur.sock
```

**Test with example client:**
```bash
./build/examples/ipc-client-example
```

**Common causes:**
- Daemon not running
- Wrong socket path in compositor config
- Permission issues

### Blur not visible

**Check compositor logs:**
- Is blur feature enabled?
- Are blur requests being sent?
- Are surfaces detected correctly?

**Test with PNG example:**
```bash
./build/examples/blur-png input.png output.png
# Does this produce blurred output?
```

**Common causes:**
- Compositor integration incomplete
- Surface detection wrong (wrong preset mapping)
- DMA-BUF import/export failing

## Getting Help

**Collect debug info:**
```bash
# Daemon version
wlblurd --version

# System info
uname -a
glxinfo | grep OpenGL

# Config
cat ~/.config/wlblur/config.toml

# Logs
wlblurd --log-level debug 2>&1 | tee wlblur-debug.log
```

**Report issue:**
- GitHub Issues: https://github.com/mecattaf/wlblur/issues
- Include debug info above
- Describe expected vs actual behavior

## See Also

- [Configuration Guide](Configuration)
- [FAQ](Getting-Started-FAQ)
- [Performance Considerations](For-Compositor-Developers-Performance-Considerations)
