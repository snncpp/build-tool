// Copyright (c) 2022 Mikael Simonsson <https://mikaelsimonsson.com>.
// SPDX-License-Identifier: BSL-1.0

#include "build-tool/validator.hh"

#include "snn-core/unittest.hh"

namespace snn
{
    void unittest()
    {
        static_assert(app::validator::is_compiler("clang++"));
        static_assert(app::validator::is_compiler("clang++9"));
        static_assert(app::validator::is_compiler("clang++10"));
        static_assert(app::validator::is_compiler("clang++99"));
        static_assert(app::validator::is_compiler("clang++-devel"));
        static_assert(app::validator::is_compiler("g++"));
        static_assert(app::validator::is_compiler("g++9"));
        static_assert(app::validator::is_compiler("g++10"));
        static_assert(app::validator::is_compiler("g++99"));
        static_assert(app::validator::is_compiler("g++-devel"));

        static_assert(!app::validator::is_compiler(""));
        static_assert(!app::validator::is_compiler("a"));
        static_assert(!app::validator::is_compiler("abc"));
        static_assert(!app::validator::is_compiler("gcc++"));
        static_assert(!app::validator::is_compiler("g++x"));
        static_assert(!app::validator::is_compiler("clang+++"));
        static_assert(!app::validator::is_compiler("clang++x"));
        static_assert(!app::validator::is_compiler("clang++123"));
        static_assert(!app::validator::is_compiler("g++123"));
        static_assert(!app::validator::is_compiler("clang++13-devel"));
        static_assert(!app::validator::is_compiler("clang++-devel13"));

        static_assert(app::validator::is_base("a"));
        static_assert(app::validator::is_base("ab"));
        static_assert(app::validator::is_base("abc"));
        static_assert(app::validator::is_base("a9"));
        static_assert(app::validator::is_base("a_8-.X"));
        static_assert(app::validator::is_base(".a"));
        static_assert(app::validator::is_base(".abc"));
        static_assert(app::validator::is_base(".abc.test"));
        static_assert(app::validator::is_base(".a-._t"));
        static_assert(!app::validator::is_base(""));
        static_assert(!app::validator::is_base("."));
        static_assert(!app::validator::is_base(".."));
        static_assert(!app::validator::is_base("a."));
        static_assert(!app::validator::is_base("a_"));
        static_assert(!app::validator::is_base("a-"));
        static_assert(!app::validator::is_base("ab."));
        static_assert(!app::validator::is_base("ab_"));
        static_assert(!app::validator::is_base("ab-"));
        static_assert(!app::validator::is_base("_a"));
        static_assert(!app::validator::is_base("-a"));
        static_assert(!app::validator::is_base(".a."));
        static_assert(!app::validator::is_base(".a-"));
        static_assert(!app::validator::is_base(".a_"));
        static_assert(!app::validator::is_base("_ab"));
        static_assert(!app::validator::is_base("-ab"));
        static_assert(!app::validator::is_base(".ab."));
        static_assert(!app::validator::is_base(".ab-"));
        static_assert(!app::validator::is_base(".ab_"));
        static_assert(!app::validator::is_base("9"));
        static_assert(!app::validator::is_base("9a"));

        static_assert(app::validator::is_directory(""));
        static_assert(app::validator::is_directory("/"));
        static_assert(app::validator::is_directory("./"));
        static_assert(app::validator::is_directory("../"));
        static_assert(app::validator::is_directory("./../"));
        static_assert(app::validator::is_directory("../../"));
        static_assert(app::validator::is_directory("a/"));
        static_assert(app::validator::is_directory("/a/"));
        static_assert(app::validator::is_directory("/.a/"));
        static_assert(app::validator::is_directory("/./a/"));
        static_assert(app::validator::is_directory("/./../a/"));
        static_assert(app::validator::is_directory("/../../a/"));

        static_assert(!app::validator::is_directory("a"));
        static_assert(!app::validator::is_directory("//a/"));
        static_assert(!app::validator::is_directory("/-/"));
        static_assert(!app::validator::is_directory("/.../"));
        static_assert(!app::validator::is_directory("/83/"));

        static_assert(app::validator::is_file_path("a"));
        static_assert(app::validator::is_file_path("/a"));
        static_assert(app::validator::is_file_path("../a"));
        static_assert(app::validator::is_file_path("../.a"));
        static_assert(!app::validator::is_file_path(""));
        static_assert(!app::validator::is_file_path("/"));
        static_assert(!app::validator::is_file_path("a/"));
        static_assert(!app::validator::is_file_path("/../"));
        static_assert(!app::validator::is_file_path("../."));

        static_assert(app::validator::is_library("a"));
        static_assert(app::validator::is_library("A"));
        static_assert(app::validator::is_library("A.b"));
        static_assert(app::validator::is_library("a9"));
        static_assert(app::validator::is_library("abc"));
        static_assert(app::validator::is_library("abc32"));
        static_assert(app::validator::is_library("aBC32"));
        static_assert(app::validator::is_library("aBC_a"));
        static_assert(app::validator::is_library("aBC.a"));
        static_assert(app::validator::is_library("aBC-a"));
        static_assert(app::validator::is_library("aBC_de"));
        static_assert(app::validator::is_library("aBC.de"));
        static_assert(app::validator::is_library("aBC-de"));
        static_assert(app::validator::is_library("aBC_32"));
        static_assert(app::validator::is_library("aBC.32"));
        static_assert(app::validator::is_library("aBC-32"));
        static_assert(app::validator::is_library("abcdefghijABCDEFGHIJabcdefghijABCDEFGHIJ"));

        static_assert(!app::validator::is_library(""));
        static_assert(!app::validator::is_library("3"));
        static_assert(!app::validator::is_library("3a"));
        static_assert(!app::validator::is_library("."));
        static_assert(!app::validator::is_library("-"));
        static_assert(!app::validator::is_library("_"));
        static_assert(!app::validator::is_library("a."));
        static_assert(!app::validator::is_library("a-"));
        static_assert(!app::validator::is_library("a_"));
        static_assert(!app::validator::is_library(".a"));
        static_assert(!app::validator::is_library("-a"));
        static_assert(!app::validator::is_library("_a"));
        static_assert(!app::validator::is_library("a b"));
        static_assert(!app::validator::is_library("åäö"));
        static_assert(!app::validator::is_library("abcdefghijABCDEFGHIJabcdefghijABCDEFGHIJx"));

        static_assert(app::validator::is_macro("__FOO__"));
        static_assert(app::validator::is_macro("BAR9"));
        static_assert(app::validator::is_macro("NDEBUG"));
        static_assert(app::validator::is_macro("snn_something_something"));

        static_assert(!app::validator::is_macro(""));
        static_assert(!app::validator::is_macro("9BAR"));
        static_assert(!app::validator::is_macro("FOO BAR9"));
        static_assert(!app::validator::is_macro("NO-DEBUG"));
        static_assert(!app::validator::is_macro(","));
        static_assert(!app::validator::is_macro(",__FOO__"));
        static_assert(!app::validator::is_macro("__FOO__,"));
        static_assert(!app::validator::is_macro("__FOO__,,BAR9"));
        static_assert(!app::validator::is_macro("1FOO,BAR9"));

        static_assert(app::validator::is_reserved_target("", "all"));
        static_assert(app::validator::is_reserved_target("", "run"));
        static_assert(app::validator::is_reserved_target("", "clean"));
        static_assert(app::validator::is_reserved_target("", "destruct"));
        static_assert(app::validator::is_reserved_target("", "minimize-corpus"));
        static_assert(app::validator::is_reserved_target("", "compress-corpus"));
        static_assert(app::validator::is_reserved_target("", "clean-executables"));
        static_assert(app::validator::is_reserved_target("", "clean-object-files"));

        static_assert(app::validator::is_reserved_target("./", "all"));
        static_assert(app::validator::is_reserved_target("./", "run"));
        static_assert(app::validator::is_reserved_target("./", "clean"));
        static_assert(app::validator::is_reserved_target("./", "destruct"));
        static_assert(app::validator::is_reserved_target("./", "minimize-corpus"));
        static_assert(app::validator::is_reserved_target("./", "compress-corpus"));
        static_assert(app::validator::is_reserved_target("./", "clean-executables"));
        static_assert(app::validator::is_reserved_target("./", "clean-object-files"));

        static_assert(!app::validator::is_reserved_target("", "abc"));
        static_assert(!app::validator::is_reserved_target("./", "abc"));
        static_assert(!app::validator::is_reserved_target("sub/", "all"));
        static_assert(!app::validator::is_reserved_target("", "RUN"));
        static_assert(!app::validator::is_reserved_target("./", "RUN"));
        static_assert(!app::validator::is_reserved_target("", "setup"));
        static_assert(!app::validator::is_reserved_target("./", "setup"));
        static_assert(!app::validator::is_reserved_target("sub/", "RUN"));
        static_assert(!app::validator::is_reserved_target("", "cleaning"));
        static_assert(!app::validator::is_reserved_target("a/", "setup"));
    }
}
