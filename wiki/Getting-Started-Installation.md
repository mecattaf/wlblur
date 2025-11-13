# Installation

## Prerequisites

wlblur requires:
- Modern Linux distribution (kernel 3.3+)
- OpenGL ES 2.0 capable GPU
- Mesa with EGL DMA-BUF support (Mesa 10.0+)

## Building from Source

### Install Dependencies

**Fedora/RHEL:**
```bash
sudo dnf install meson ninja-build gcc pkg-config
sudo dnf install mesa-libEGL-devel mesa-libGLES-devel libdrm-devel
sudo dnf install tomlc99-devel
```

**Ubuntu/Debian:**
```bash
sudo apt install meson ninja-build gcc pkg-config
sudo apt install libegl1-mesa-dev libgles2-mesa-dev libdrm-dev
sudo apt install libtoml-dev
```

**Arch Linux:**
```bash
sudo pacman -S meson ninja gcc pkgconf
sudo pacman -S mesa libdrm
sudo pacman -S tomlc99
```

### Build Steps

```bash
# Clone repository
git clone https://github.com/mecattaf/wlblur.git
cd wlblur

# Configure build
meson setup build

# Compile
meson compile -C build

# Install (optional)
sudo meson install -C build
```

### Verify Installation

```bash
# Check daemon binary
./build/wlblurd/wlblurd --help

# Test with example
./build/examples/blur-png test.png output.png
```

## Package Installation

*(Coming soon - packages for major distributions)*

## Next Steps

- [Quick Start Guide](Quick-Start) - Set up configuration
- [Configuration](User-Guide-Configuration) - Detailed config options
- [What is wlblur?](What-is-wlblur) - Learn more about the project
