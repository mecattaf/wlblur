# Building from Source

## Prerequisites

### Required Dependencies

- **Build tools**: meson (≥0.60), ninja, gcc/clang, pkg-config
- **Graphics**: EGL, OpenGL ES 2.0, libdrm
- **Configuration**: tomlc99
- **System**: Linux kernel 3.3+ (for DMA-BUF support)

### Optional Dependencies

- **Testing**: check, cmocka
- **Documentation**: doxygen, sphinx
- **Examples**: libpng

## Installation

### Fedora/RHEL

```bash
sudo dnf install meson ninja-build gcc pkg-config
sudo dnf install mesa-libEGL-devel mesa-libGLES-devel libdrm-devel
sudo dnf install tomlc99-devel
sudo dnf install check-devel  # Optional: for tests
```

### Ubuntu/Debian

```bash
sudo apt install meson ninja-build gcc pkg-config
sudo apt install libegl1-mesa-dev libgles2-mesa-dev libdrm-dev
sudo apt install libtoml-dev
sudo apt install check  # Optional: for tests
```

### Arch Linux

```bash
sudo pacman -S meson ninja gcc pkgconf
sudo pacman -S mesa libdrm
sudo pacman -S tomlc99
sudo pacman -S check  # Optional: for tests
```

## Build Steps

### Standard Build

```bash
# Clone repository
git clone https://github.com/mecattaf/wlblur.git
cd wlblur

# Configure
meson setup build

# Compile
meson compile -C build

# Run tests
meson test -C build

# Install (optional)
sudo meson install -C build
```

### Development Build

```bash
# Debug build with sanitizers
meson setup build \
    -Dbuildtype=debug \
    -Db_sanitize=address,undefined

meson compile -C build
```

### Release Build

```bash
# Optimized release build
meson setup build \
    -Dbuildtype=release \
    -Doptimization=3

meson compile -C build
```

## Build Options

View all options:
```bash
meson configure build
```

Common options:
- `-Dbuildtype=debug|release` - Build type
- `-Doptimization=0|1|2|3` - Optimization level
- `-Db_sanitize=address,undefined` - Enable sanitizers
- `-Dtests=true|false` - Build tests
- `-Dexamples=true|false` - Build examples

## Troubleshooting

### Missing Dependencies

**Error:** `dependency 'egl' not found`

**Solution:** Install Mesa EGL development headers

### Build Fails

**Check dependency versions:**
```bash
pkg-config --modversion egl
pkg-config --modversion glesv2
```

**Clean build directory:**
```bash
rm -rf build
meson setup build
```

### Runtime Issues

**Check library paths:**
```bash
ldd build/wlblurd/wlblurd
```

**Set library path:**
```bash
export LD_LIBRARY_PATH=$PWD/build/libwlblur:$LD_LIBRARY_PATH
```

## Verification

```bash
# Check daemon binary
./build/wlblurd/wlblurd --version
./build/wlblurd/wlblurd --help

# Run examples
./build/examples/blur-png test.png output.png
./build/examples/ipc-client-example

# Run tests
meson test -C build -v
```

## Next Steps

- [Contributing](Contributing) - How to contribute
- [Testing](Testing) - Test procedures
- [Code Style](Code-Style) - Coding conventions
