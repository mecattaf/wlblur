# Testing

## Test Structure

wlblur uses a multi-layered testing approach:

1. **Unit tests** - Test individual functions
2. **Integration tests** - Test component interactions
3. **Example programs** - Manual testing and demos
4. **Performance tests** - Benchmark blur performance

## Running Tests

### All Tests

```bash
meson test -C build
```

### Specific Test Suite

```bash
meson test -C build --suite libwlblur
meson test -C build --suite wlblurd
meson test -C build --suite config
```

### Verbose Output

```bash
meson test -C build -v
```

### Failed Tests Only

```bash
meson test -C build --print-errorlogs
```

## Unit Tests

### libwlblur Tests

Test core blur algorithms:
```bash
./build/tests/test_kawase
./build/tests/test_dmabuf
./build/tests/test_egl_context
```

### wlblurd Tests

Test daemon functionality:
```bash
./build/tests/test_ipc_protocol
./build/tests/test_blur_nodes
./build/tests/test_config_parser
```

## Integration Tests

Test full workflows:
```bash
# Start daemon
./build/wlblurd/wlblurd &
DAEMON_PID=$!

# Run integration test
./build/tests/test_integration

# Cleanup
kill $DAEMON_PID
```

## Example Programs

### blur-png

Test blur on PNG images:
```bash
./build/examples/blur-png input.png output.png

# With custom parameters
./build/examples/blur-png input.png output.png --radius 10 --passes 4
```

### ipc-client-example

Test IPC communication:
```bash
# Start daemon
./build/wlblurd/wlblurd &

# Run example
./build/examples/ipc-client-example

# Should output:
# Connected to daemon
# Created blur node: 1
# Blur request successful
```

## Performance Tests

### Benchmark Tool

```bash
./build/tests/benchmark_blur

# Output:
# Kawase 3 passes @ 1920x1080: 1.23ms
# Kawase 5 passes @ 1920x1080: 2.01ms
# Kawase 3 passes @ 3840x2160: 4.89ms
```

### Profile with perf

```bash
perf record -g ./build/tests/benchmark_blur
perf report
```

## Memory Testing

### Valgrind

Check for memory leaks:
```bash
valgrind --leak-check=full --show-leak-kinds=all \
    ./build/wlblurd/wlblurd --test-mode

# Should show:
# All heap blocks were freed -- no leaks are possible
```

### AddressSanitizer

Build with sanitizers:
```bash
meson setup build -Db_sanitize=address,undefined
meson compile -C build
meson test -C build
```

## Writing Tests

### Unit Test Example

```c
#include <check.h>
#include "blur_internal.h"

START_TEST(test_kawase_downsample) {
    struct blur_config config = {
        .radius = 5.0,
        .passes = 3,
    };

    int result = kawase_downsample(&config);
    ck_assert_int_eq(result, 0);
}
END_TEST

Suite *kawase_suite(void) {
    Suite *s = suite_create("Kawase");
    TCase *tc = tcase_create("Core");

    tcase_add_test(tc, test_kawase_downsample);
    suite_add_tcase(s, tc);

    return s;
}

int main(void) {
    Suite *s = kawase_suite();
    SRunner *sr = srunner_create(s);

    srunner_run_all(sr, CK_NORMAL);
    int failed = srunner_ntests_failed(sr);
    srunner_free(sr);

    return (failed == 0) ? 0 : 1;
}
```

### Adding to Build System

In `tests/meson.build`:
```meson
test_kawase = executable('test_kawase',
    'test_kawase.c',
    dependencies: [check, libwlblur_dep],
)

test('kawase', test_kawase, suite: 'libwlblur')
```

## Continuous Integration

Tests run automatically on:
- Pull requests
- Commits to main
- Release tags

CI checks:
- [ ] All tests pass
- [ ] No compiler warnings
- [ ] No memory leaks (valgrind)
- [ ] Code coverage >80%

## Test Coverage

### Generate Coverage Report

```bash
meson setup build -Db_coverage=true
meson compile -C build
meson test -C build
ninja -C build coverage
```

View report:
```bash
# HTML report
firefox build/meson-logs/coveragereport/index.html

# Text summary
cat build/meson-logs/coverage.txt
```

## Troubleshooting Tests

### Test Fails

```bash
# Run with verbose output
meson test -C build -v test_name

# Run directly
./build/tests/test_name

# Use debugger
gdb ./build/tests/test_name
```

### Test Hangs

```bash
# Run with timeout
timeout 10s ./build/tests/test_name

# Check for infinite loops or deadlocks
```

## See Also

- [Contributing](Contributing) - How to contribute
- [Building from Source](Building-from-Source) - Build setup
- [Code Style](Code-Style) - Coding conventions
