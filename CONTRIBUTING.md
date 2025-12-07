# Contributing to UDS Standard

Thank you for your interest in contributing to the UDS Standard library!

## Getting Started

1. Fork the repository
2. Clone your fork: `git clone https://github.com/rbrtjns90/UDS_standard.git`
3. Create a feature branch: `git checkout -b feature/your-feature`

## Building

```bash
# Build the library
make lib

# Build and run all tests
make test-all

# Quick test iteration (Google Tests only)
make test-quick
```

## Testing

We have comprehensive test coverage. Please ensure all tests pass before submitting:

```bash
# Run all tests (Google Test + Legacy + Fuzz)
make test-all

# Run only Google Tests (faster)
make test-quick

# Run with sanitizers
make asan    # AddressSanitizer
make ubsan   # UndefinedBehaviorSanitizer
```

### Adding Tests

- Add unit tests to `tests/gtest/` using Google Test framework
- Test files should be named `*_test.cpp`
- Follow existing test patterns for consistency

## Code Style

- C++17 standard
- 120 character line limit
- Use `clang-format` if available
- Follow existing code patterns

## Pull Request Process

1. Ensure all tests pass: `make test-all`
2. Update documentation if needed
3. Add tests for new functionality
4. Submit PR against `main` branch

## Reporting Issues

- Use GitHub Issues
- Include minimal reproduction steps
- Specify your OS and compiler version

## License

By contributing, you agree that your contributions will be licensed under the same license as the project.
