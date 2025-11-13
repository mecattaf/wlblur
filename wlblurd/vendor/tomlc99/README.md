# tomlc99 - Bundled Dependency

**Source:** https://github.com/cktan/tomlc99
**Commit:** 26b9c1ea770dab2378e5041b695d24ccebe58a7a
**License:** MIT

## About

tomlc99 is a TOML v1.0.0-compliant parser written in C99 by CK Tan.

## Why Bundled?

This dependency is bundled directly in the wlblur repository for the following reasons:

1. **Distribution Compatibility**: tomlc99 is not packaged on many Linux distributions (notably Fedora, which is the maintainer's primary development environment)
2. **Build Simplicity**: Eliminates external dependency requirements for users building from source
3. **Small Footprint**: Only 2 files (~500 lines total), making bundling practical
4. **Stable API**: TOML v1.0.0 specification is stable, minimal risk of breaking changes

## Files Included

- `toml.h` - Header file
- `toml.c` - Implementation
- `LICENSE` - MIT license text
- `README.md` - This file

## License

tomlc99 is licensed under the MIT License. See LICENSE file for full text.

Copyright (c) CK Tan
https://github.com/cktan/tomlc99
