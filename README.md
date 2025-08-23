# **C**ore**u**tils fanci**fied** (Cu-fied)

Cu-fied (_pronounced /kjufaɪd/_ ) is a rewrite of [GNU's Coreutils](https://www.gnu.org/software/coreutils/) to focus on being "fancified" rather than being machine-readable.
(Fancier in my subjective opinion.)

## Building from source

First off, it is primarily designed for the x86 and ARM platforms of linux and macOS. It most definitely will not work on Windows due to using `unistd.h` and some specific posix functions; However, WSL and Cygwin _might_ work. Supporting Windows is not on the roadmap, although PRs to improve cross-platform compatibility are welcome.

With that out of the way, you only need these dependencies:

- [Zig](https://ziglang.org/) 0.15.1 or later.
- [cURL](https://github.com/curl/curl) installed via a system package manager.
  (Note that it is planned to statically link cURL if it is not installed globally.)

### Helpful commands

```sh
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
