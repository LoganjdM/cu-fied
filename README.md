# **C**ore**u**tils fanci**fied** (Cu-fied)

Cu-fied (_pronounced /kjufaɪd/_ ) is a rewrite of [GNU's Coreutils](https://www.gnu.org/software/coreutils/) to focus on being "fancified" rather than being machine-readable.
(Fancier in my subjective opinion.)

## Building from source

First off, it is primarily designed for x86\_64 GNU/linux and aarch64 macOS. It most definitely will not work on Windows due to using `unistd.h` and some specific posix functions; However, WSL and Cygwin _might_ work. Supporting Windows is not on the roadmap, although PRs to improve cross-platform compatibility are welcome.

With that out of the way, you only need these dependencies:

- [Zig](https://ziglang.org/) 0.14.0 or later (0.14.0 is needed for proper C23 support.)

### Helpful commands

```sh
# Build the project
$ zig build

# Run a specific program, e.g. `lsf`
$ zig build run-<program>

# Generate man pages (requires help2man to be installed on your system)
$ zig build help2man
```
