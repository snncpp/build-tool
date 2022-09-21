# Build tool

The [snncpp][snncpp] framework build tool can build C++ projects that follow the same naming convention and
directory structure as [snn-core][snncore]. It understands simple preprocessing directives ([example](https://github.com/snncpp/snn-core/blob/master/crypto/hash/sha256.hh))
and will link with libraries listed in `#include` comments ([example](https://github.com/snncpp/snn-core/blob/master/crypto/hash/impl/sha256.openssl.hh)).

The build tool executable is `snn` (by default), run it without any arguments to see what commands
are available:

```console
$ snn
Usage: snn <command> [arguments]

Commands:
build   Build one or more applications
gen     Generate a makefile for one or more applications
run     Build and run a single application with optional arguments
runall  Build and run one or more applications

For more information run a command without arguments, e.g.:
snn build
```

Run a command without arguments for more information:

```console
$ snn runall
Usage: snn runall [options] [--] app.cc [...]

Options:
-o --optimize            Optimize (-O2)
-t --time-execution      Time command execution (implies verbose)
-s --sanitize            Enable sanitizers (Address & UndefinedBehavior)
-c --compiler compiler   Compiler (default: clang++)
-d --define MACRO[,...]  Define macro(s)
-v --verbose             Increase verbosity (up to three times)

Verbosity levels:
1. Show compile/run commands
2. Show all commands
3. Debug
```

For example, to run all unit tests in the [snn-core/pair/](https://github.com/snncpp/snn-core/tree/master/pair) subdirectory:

```console
$ snn runall --verbose snn-core/pair/*.test.cc
clang++ --config ./.clang -iquote ../ -c -o pair/common.test.o pair/common.test.cc
clang++ --config ./.clang -o pair/common.test pair/common.test.o -L/usr/local/lib/
clang++ --config ./.clang -iquote ../ -c -o pair/core.test.o pair/core.test.cc
clang++ --config ./.clang -o pair/core.test pair/core.test.o -L/usr/local/lib/
./pair/common.test
./pair/core.test
```


## Officially supported platforms

The [snncpp][snncpp] framework currently targets [POSIX][posix] and is developed and tested on:

| Operating system     | Compiler             |
| -------------------- | -------------------- |
| FreeBSD 13.1         | Clang 13+            |
| Fedora Linux 36      | Clang 13+            |


## Getting started

### Install Clang and libc++ (and optional dependencies)

#### FreeBSD 13.1

FreeBSD 13.1 comes with Clang and libc++ version 13, so no software installation is needed.

Optional dependencies, as root (or with `sudo`):

```console
# pkg install pcre2
```

#### Fedora Linux 36

Install Clang and libc++, as root (or with `sudo`):

```console
# dnf install clang libcxx-devel
```

Optional dependencies, as root (or with `sudo`):

```console
# dnf install openssl-devel pcre2-devel
```

### Clone and build

1. Clone [build-tool][buildtool] and [snn-core][snncore] from the same directory, here we use `~/project/cpp`:

```console
$ mkdir -p ~/project/cpp
$ cd ~/project/cpp
$ git clone https://github.com/snncpp/snn-core.git
Cloning into 'snn-core'...
$ git clone https://github.com/snncpp/build-tool.git
Cloning into 'build-tool'...
```

2. Copy `.clang` config from `snn-core` to `~/project/cpp`:

```console
$ cp -i snn-core/.clang .
```

3. Build:

```console
$ cd build-tool
$ make
clang++ --config ../.clang -O2 -iquote ../ -c -o snn.o snn.cc
clang++ --config ../.clang -O2 -o snn snn.o
```

4. Copy the `snn` binary to your home directory (or preferably put it somewhere in your `$PATH`):

```console
$ cp -i snn ~/
```


## Your first application

After following [Getting started](#getting-started) above, you can now create your first application:

```console
$ cd ~/project/cpp
$ mkdir myapp
$ cp -i snn-core/.template/main.cc myapp/myapp.cc
$ cd myapp
$ ~/snn run myapp.cc
Hello!
```


## Fuzzing

The build tool can generate makefiles for fuzzing. Here we run the fuzzer for `base64::decode(...)`.

```console
$ cd snn-core/base64/detail
$ ls -1
decode.fuzz.cc
decode.fuzz.corpus.tar.gz
encode.fuzz.cc
encode.fuzz.corpus.tar.gz
$ ~/snn gen --fuzz decode.fuzz.cc
$ make run
clang++ --config ../../.clang -fsanitize=fuzzer,address,undefined,integer -fno-sanitize-recover=all -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -iquote ../../../ -c -o decode.fuzz.o decode.fuzz.cc
clang++ --config ../../.clang -fsanitize=fuzzer,address,undefined,integer -fno-sanitize-recover=all -DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION -o decode.fuzz decode.fuzz.o -L/usr/local/lib/
tar -xzf decode.fuzz.corpus.tar.gz
./decode.fuzz -rss_limit_mb=3072 -timeout=5 decode.fuzz.corpus/
INFO: Running with entropic power schedule (0xFF, 100).
INFO: Seed: 4112589604
INFO: Loaded 1 modules   (705 inline 8-bit counters): 705 [0x329841, 0x329b02),
INFO: Loaded 1 PC tables (705 PCs): 705 [0x2303b8,0x232fc8),
INFO:       36 files found in decode.fuzz.corpus/
INFO: -max_len is not provided; libFuzzer will not generate inputs larger than 4096 bytes
INFO: seed corpus: files: 36 min: 1b max: 260b total: 1509b rss: 13Mb
#37     INITED cov: 240 ft: 555 corp: 36/1509b exec/s: 0 rss: 32Mb
#131072 pulse  cov: 240 ft: 555 corp: 36/1509b lim: 1559 exec/s: 43690 rss: 63Mb
#262144 pulse  cov: 240 ft: 555 corp: 36/1509b lim: 2863 exec/s: 43690 rss: 92Mb
...
```

Additional fuzzing targets include `minimize-corpus` and `compress-corpus`.


## License

See [LICENSE](LICENSE). Copyright © 2022 [Mikael Simonsson](https://mikaelsimonsson.com).


[buildtool]: https://github.com/snncpp/build-tool
[posix]: https://en.wikipedia.org/wiki/POSIX  "Portable Operating System Interface"
[snncore]: https://github.com/snncpp/snn-core
[snncpp]: https://github.com/snncpp
