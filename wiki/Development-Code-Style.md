# Code Style

## General Principles

- **Clarity over cleverness**: Write code that's easy to understand
- **Consistency**: Follow existing patterns in the codebase
- **Documentation**: Comment complex logic, document public APIs
- **Testing**: Write tests for new features and bug fixes

## C Code Style

### Formatting

- **Indentation**: 4 spaces (no tabs)
- **Line length**: 80-100 characters (flexible)
- **Braces**: K&R style (opening brace on same line)

```c
// Good
if (condition) {
    do_something();
} else {
    do_something_else();
}

// Bad
if (condition)
{
    do_something();
}
```

### Naming

- **Functions**: `snake_case` - `blur_render_kawase()`
- **Variables**: `snake_case` - `blur_node_count`
- **Constants**: `UPPER_CASE` - `MAX_BLUR_PASSES`
- **Types/Structs**: `snake_case` with suffix - `struct blur_config`

```c
struct blur_config {
    float radius;
    int passes;
};

#define MAX_BLUR_PASSES 8

int blur_node_count = 0;

void blur_render_kawase(struct blur_config *config) {
    // ...
}
```

### Comments

```c
// Single-line comment for brief explanations

/*
 * Multi-line comment for detailed explanations:
 * - Describes complex algorithms
 * - Documents public API functions
 * - Explains non-obvious design decisions
 */

/**
 * Public API documentation (Doxygen style)
 *
 * @param config Blur configuration
 * @return 0 on success, -1 on error
 */
int blur_init(struct blur_config *config);
```

### Error Handling

```c
// Return error codes
int blur_init(void) {
    if (!egl_init()) {
        return -1;
    }
    return 0;
}

// Use goto for cleanup
int blur_render(void) {
    GLuint tex = 0;
    int ret = -1;

    tex = create_texture();
    if (!tex) {
        goto cleanup;
    }

    // ... render ...

    ret = 0;

cleanup:
    if (tex) {
        glDeleteTextures(1, &tex);
    }
    return ret;
}
```

## File Organization

### Header Files

```c
#ifndef BLUR_INTERNAL_H
#define BLUR_INTERNAL_H

#include <stdint.h>

// Forward declarations
struct blur_node;

// Type definitions
typedef void (*blur_callback_t)(void *data);

// Function declarations
void blur_init(void);
void blur_fini(void);

#endif
```

### Source Files

```c
#include "blur_internal.h"

#include <stdlib.h>
#include <string.h>

// Static helpers first
static inline bool is_power_of_two(int n) {
    return (n & (n - 1)) == 0;
}

// Public API implementations
void blur_init(void) {
    // ...
}
```

## Git Commit Messages

### Format

```
type: Brief description (50 chars max)

Detailed explanation of what and why (wrap at 72 chars).
Include context, rationale, and any important details.

Fixes: #123
```

### Types

- `feat`: New feature
- `fix`: Bug fix
- `docs`: Documentation only
- `style`: Formatting, no code change
- `refactor`: Code refactoring
- `perf`: Performance improvement
- `test`: Add/update tests
- `chore`: Maintenance tasks

### Examples

```
feat: add Gaussian blur algorithm

Implement separable Gaussian blur as alternative to Kawase.
Provides higher quality at cost of performance.

- Add gaussian_blur.c implementation
- Add tests for Gaussian blur
- Update configuration to support algorithm selection

Fixes: #42
```

```
fix: prevent memory leak in blur_node_destroy

Free all GPU resources when destroying blur nodes.
Previously leaked framebuffer objects.

- Add proper glDeleteFramebuffers call
- Add unit test for resource cleanup
```

## Code Review

When reviewing code, check for:
- [ ] Follows style guidelines
- [ ] Has appropriate tests
- [ ] Documentation updated
- [ ] No compiler warnings
- [ ] Error handling present
- [ ] Memory leaks checked (valgrind)
- [ ] Performance considered

## Tools

### Format Code

```bash
# Use clang-format (if available)
clang-format -i src/**/*.c include/**/*.h
```

### Check Style

```bash
# Compile with warnings
meson setup build -Dwarning_level=3
meson compile -C build

# Should have zero warnings
```

### Static Analysis

```bash
# Use cppcheck
cppcheck --enable=all src/

# Use clang-tidy
clang-tidy src/**/*.c
```

## See Also

- [Contributing](Contributing) - How to contribute
- [Building from Source](Building-from-Source) - Build instructions
- [Testing](Testing) - Test procedures
