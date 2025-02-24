# **C**ore**u**tils fanci**fied** (Cu-fied)

Cu-fied (_pronounced /kjufaɪd/_ ) is a rewrite of [GNU's Coreutils](https://www.gnu.org/software/coreutils/) to focus on being "fancified" rather than being machine-readable.
(Fancier in my subjective opinion.)

## Building from source

First off, it is primarily designed for x86\__64_ linux, although it _is_ tested to work with macOS. It most definitely will not work on Windows due using `unistd.h`. Supporting Windows is not on the roadmap, although PRs to improve cross-platform compatibility are welcome.

With that out of the way, you only need these dependencies:

- [Zig](https://ziglang.org/) 0.14.0 or later (Zig 0.14.0 is needed for proper C23 support. Notably, it’s not stable yet, but it will be within a week or two.)

### Helpful commands

```sh
# Build the project
$ zig build

# Run a specific program, e.g. `lsf`
$ zig build run-<program>
```
