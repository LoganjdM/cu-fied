# Core Utils fanciFied (Cu-Fied)

Cu-fied (*pronounced /kjufaɪd/* ) is a collection of my own implementations of [GNU's Core Utils](https://www.gnu.org/software/coreutils/) made to be "fancy" or "fancified".
###### In my subjective opinion.

# Building from source

First off this is mostly a side project for me and me only, so it is only designed for x86_64 linux. It *is* tested to work with MacOS but with some issues(notably compiling with `zig cc` since I couldn't get access to a c23 compiler): It most definitely will not work on Windows due to the use of `unistd.h`. There are also no plans to suppourt Windows either.

With that out of the way you simply just need
- A modern C compiler(C23 standard), *preferrably* GCC15 or a modern release of Clang
###### (`zig cc` does work but to my knowledge it is just a Clang frontend)
- The [Zig](https://ziglang.org/) compiler

And building is as simple as running
```console
$ cc nob.c -o nob
```
for the builder as we make use of [NoB](https://github.com/tsoding/nob.h) and
```console
$ ./nob
```
to build, with the resulting binaries either in `bin/` for release or `dev/` for development.