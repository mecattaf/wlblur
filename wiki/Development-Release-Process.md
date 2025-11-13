# Release Process

## Overview

wlblur follows semantic versioning (SemVer): `MAJOR.MINOR.PATCH`

- **MAJOR**: Incompatible API changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes (backward compatible)

## Release Checklist

### Pre-Release

- [ ] All tests passing on main
- [ ] No known critical bugs
- [ ] Documentation updated
- [ ] CHANGELOG.md updated
- [ ] Version numbers updated
- [ ] Performance benchmarks run

### Release Steps

1. **Update Version**

```bash
# In meson.build
project('wlblur', 'c',
  version: '1.2.0',  # Update version
  ...
)
```

2. **Update CHANGELOG**

```markdown
## [1.2.0] - 2025-01-15

### Added
- Gaussian blur algorithm
- Material system support
- Hot reload via SIGUSR1

### Fixed
- Memory leak in blur_node_destroy
- DMA-BUF format negotiation

### Changed
- Default blur passes from 3 to 2 (performance)

### Deprecated
- Old config format (use TOML)
```

3. **Create Release Commit**

```bash
git add meson.build CHANGELOG.md
git commit -m "Release v1.2.0"
```

4. **Tag Release**

```bash
git tag -a v1.2.0 -m "Release v1.2.0"
git push origin v1.2.0
```

5. **Build Release Artifacts**

```bash
meson setup build -Dbuildtype=release
meson compile -C build
meson dist -C build
```

6. **Create GitHub Release**

- Go to GitHub Releases
- Create new release from tag
- Attach tarball from `build/meson-dist/`
- Copy CHANGELOG entry to release notes

### Post-Release

- [ ] Announce on social media
- [ ] Update documentation website
- [ ] Notify package maintainers
- [ ] Bump version to next -dev

## Version Numbering

### Development Version

Between releases:
```meson
version: '1.3.0-dev'
```

### Release Candidate

Before major releases:
```meson
version: '2.0.0-rc1'
```

### Stable Release

Official releases:
```meson
version: '1.2.0'
```

## Release Schedule

- **Patch releases**: As needed (bug fixes)
- **Minor releases**: Every 2-3 months (new features)
- **Major releases**: When breaking changes needed

## Supported Versions

| Version | Status | End of Support |
|---------|--------|----------------|
| 1.2.x | Current | - |
| 1.1.x | Maintained | 2025-06-01 |
| 1.0.x | Security only | 2025-03-01 |

## Package Distribution

### Source Tarball

```bash
meson dist -C build
# Creates: wlblur-1.2.0.tar.xz
```

### Distribution Packages

**Fedora/RHEL:**
```bash
# Maintainer builds RPM
rpmbuild -ta wlblur-1.2.0.tar.xz
```

**Ubuntu/Debian:**
```bash
# Maintainer builds deb
debuild -us -uc
```

**Arch Linux:**
```bash
# AUR package updated
# https://aur.archlinux.org/packages/wlblur
```

## Release Notes Template

```markdown
# wlblur v1.2.0

Released: 2025-01-15

## Highlights

- **New Feature**: Gaussian blur algorithm
- **Performance**: 20% faster blur on integrated GPUs
- **Configuration**: Hot reload support

## Changes

See [CHANGELOG.md](CHANGELOG) for complete list.

## Installation

### From Source
\`\`\`bash
wget https://github.com/mecattaf/wlblur/archive/v1.2.0.tar.gz
tar xf v1.2.0.tar.gz
cd wlblur-1.2.0
meson setup build
meson compile -C build
sudo meson install -C build
\`\`\`

### Packages

- Fedora: `sudo dnf install wlblur`
- Arch: `yay -S wlblur`
- Ubuntu: Coming soon

## Upgrade Notes

**Breaking Changes:**
- None

**Deprecations:**
- Old config format (migrate to TOML)

## Known Issues

- #123: Blur flickers on some AMD GPUs (workaround available)

## Contributors

Thanks to all contributors for this release!
```

## Hotfix Process

For critical bugs:

1. Create hotfix branch from release tag
```bash
git checkout -b hotfix/1.2.1 v1.2.0
```

2. Fix bug and test
3. Update version to `1.2.1`
4. Tag and release
5. Merge back to main

## See Also

- [Contributing](Contributing) - How to contribute
- [Testing](Testing) - Test procedures
- [Project Roadmap](Roadmap-Project-Roadmap) - Future plans
