# Contributing

Thank you for your interest in contributing to wlblur!

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/YOUR_USERNAME/wlblur.git`
3. Create a branch: `git checkout -b feature/your-feature`
4. Make your changes
5. Test thoroughly
6. Commit with descriptive messages
7. Push and create a pull request

## Development Setup

See [Building from Source](Building-from-Source) for detailed build instructions.

Quick start:
```bash
git clone https://github.com/mecattaf/wlblur.git
cd wlblur
meson setup build
meson compile -C build
```

## Code Style

See [Code Style](Code-Style) for formatting guidelines.

Key points:
- Follow existing code style
- Use meaningful variable names
- Add comments for complex logic
- Keep functions focused and small

## Testing

See [Testing](Testing) for test procedures.

Run tests before submitting:
```bash
meson test -C build
```

## Pull Request Guidelines

### Title Format
- `feat: Add new feature` - New features
- `fix: Fix bug in component` - Bug fixes
- `docs: Update documentation` - Documentation
- `refactor: Refactor component` - Code refactoring
- `test: Add tests for component` - Tests
- `chore: Update build system` - Maintenance

### Description
- Explain what and why, not how
- Link related issues
- Include test results
- Note breaking changes

### Checklist
- [ ] Code follows style guidelines
- [ ] Tests added/updated
- [ ] Documentation updated
- [ ] Commit messages are descriptive
- [ ] No warnings on compilation
- [ ] All tests pass

## Areas for Contribution

### High Priority
- Compositor integrations (ScrollWM, niri, Sway)
- Performance optimizations
- Documentation improvements
- Bug fixes

### Medium Priority
- Additional blur algorithms (Gaussian, Box, Bokeh)
- Material system enhancements
- Wayland protocol extension
- Cross-compositor testing

### Low Priority
- macOS-style materials
- Advanced effects (noise, vibrancy tuning)
- Shader optimizations

## Communication

- **Issues**: Bug reports and feature requests
- **Discussions**: General questions and ideas
- **Pull Requests**: Code contributions

## Code of Conduct

Be respectful, inclusive, and constructive.

## License

By contributing, you agree your contributions will be licensed under MIT License.

## See Also

- [Building from Source](Building-from-Source)
- [Code Style](Code-Style)
- [Testing](Testing)
- [Architecture Overview](../Architecture/System-Overview)
