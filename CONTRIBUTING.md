# Contributing to sayan-compress-lib

Thank you for your interest in contributing to this high-performance C++ compression library! This document provides guidelines and instructions for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Getting Started](#getting-started)
- [Development Setup](#development-setup)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Submitting Changes](#submitting-changes)
- [Issue Reporting](#issue-reporting)

## Code of Conduct

Please be respectful and constructive in all interactions. We welcome contributions from developers of all skill levels and backgrounds.

## Getting Started

1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR_USERNAME/sayan-compress-lib.git
   cd sayan-compress-lib
   ```
3. **Add the upstream remote** to keep your fork synchronized:
   ```bash
   git remote add upstream https://github.com/ORIGINAL_OWNER/sayan-compress-lib.git
   ```
4. **Create a branch** for your changes:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## Development Setup

### Prerequisites

- C++20 compatible compiler (GCC 10+, Clang 11+, MSVC 2019+)
- CMake 3.16 or higher
- Git

### Building the Project

```bash
mkdir build && cd build
cmake ..
cmake --build .
```

### Running Tests

After building, run the test executables:

```bash
./test_basic
./test_debug
./test_simple
```

All tests should pass before submitting changes.

## Coding Standards

### C++ Style Guidelines

1. **Use C++20 features** where appropriate
2. **Zero dependencies**: Only use the C++ standard library
3. **Performance-first**: Optimize for speed and memory efficiency
4. **Clear naming**: Use descriptive variable and function names

### Code Organization

- Header-only library design (`.hpp` files)
- Use namespaces appropriately (e.g., `compress::`)
- Document public APIs with comments

### Example Code Style

```cpp
#include <vector>
#include <cstdint>

namespace compress {

/**
 * @brief Compresses input data using LZ77 + Huffman algorithm
 * @param data Input data to compress
 * @return Compressed data as byte vector
 */
std::vector<uint8_t> compress(const std::vector<uint8_t>& data) {
    // Implementation here
}

} // namespace compress
```

### Documentation

- Add comments for all public functions and classes
- Explain algorithmic choices in complex sections
- Include usage examples where helpful

## Testing

### Writing Tests

When adding new features, include corresponding tests:

1. Create test cases that cover normal operation
2. Include edge cases (empty input, large input, etc.)
3. Verify both compression and decompression
4. Check data integrity after round-trip

### Test Categories

- **Basic tests**: Core functionality verification
- **Debug tests**: Detailed debugging and profiling
- **Simple tests**: Quick sanity checks

### Running All Tests

```bash
cd build
./test_basic && ./test_debug && ./test_simple
```

## Submitting Changes

### Commit Messages

Write clear, descriptive commit messages:

```
feat: Add canonical Huffman coding optimization

- Improve encoding speed by 15%
- Reduce memory usage during tree construction
- Add unit tests for edge cases
```

### Pull Request Process

1. **Update documentation** if you change behavior or add features
2. **Run all tests** and ensure they pass
3. **Rebase on main** to incorporate latest changes:
   ```bash
   git fetch upstream
   git rebase upstream/main
   ```
4. **Push to your fork**:
   ```bash
   git push origin feature/your-feature-name
   ```
5. **Open a Pull Request** on GitHub with:
   - Clear description of changes
   - Reference any related issues
   - List testing performed

### PR Checklist

Before submitting your PR, ensure:

- [ ] Code follows project style guidelines
- [ ] All tests pass
- [ ] Documentation is updated
- [ ] Commit messages are clear and descriptive
- [ ] Changes are focused and atomic

## Issue Reporting

### Bug Reports

When reporting bugs, include:

1. **System information**: OS, compiler version, CMake version
2. **Steps to reproduce**: Clear, minimal reproduction steps
3. **Expected behavior**: What should happen
4. **Actual behavior**: What actually happens
5. **Error messages**: Full error output if applicable

### Feature Requests

For feature requests, describe:

1. **Use case**: Why this feature would be useful
2. **Proposed solution**: How you envision it working
3. **Alternatives considered**: Other approaches you've thought about

## Areas for Contribution

We welcome contributions in these areas:

- **Algorithm improvements**: LZ77 or Huffman optimizations
- **Bug fixes**: Any correctness issues
- **Documentation**: Clarifications and examples
- **Tests**: Additional test coverage
- **Performance**: Benchmarks and optimizations

## Questions?

If you have questions about contributing, please open an issue for discussion before starting major work.

---

Thank you for contributing to sayan-compress-lib!
