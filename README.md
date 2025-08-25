# **C**ore**u**tils fanci**fied** (Cu-fied)

Cu-fied (_pronounced /kjufaɪd/_ ) is a rewrite of [GNU's Coreutils](https://www.gnu.org/software/coreutils/) to focus on being "fancified" rather than being machine-readable.
(Fancier in my subjective opinion.)

## Building from source

First off, it is primarily designed for the x64 and ARM platforms of Linux and macOS.
Some of the CLIs most definitely do not work on Windows due to using `unistd.h` and specific POSIX functions; WSL and Cygwin _ought to_ work, but are not tested.
PRs to improve to improve cross-platform compatibility are welcome.

With that out of the way, you only need these dependencies:

- [Zig](https://ziglang.org/) 0.15.1 or later.
- [help2man](https://www.gnu.org/software/help2man/) (if generating documentation)

### Helpful commands

```j
# Build all of the programs
$ zig build

# Build the project for release
$ zig build --release=safe -Dcpu=baseline -Demit-man-pages

# Run a specific program, e.g. `lsf`
$ zig build run-<program>

# Generate man pages as well (requires help2man to be installed on your system)
$ zig build -Demit-man-pages

# Format project
$ zig build fmt

# Synchronize shared information
$ zig build write-info
```
