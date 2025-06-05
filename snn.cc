// Copyright (c) 2022 Mikael Simonsson <https://mikaelsimonsson.com>.
// SPDX-License-Identifier: BSL-1.0

#include "snn-core/exception.hh"
#include "snn-core/main.hh"
#include "snn-core/vec.hh"
#include "snn-core/algo/join.hh"
#include "snn-core/ascii/trim.hh"
#include "snn-core/chr/common.hh"
#include "snn-core/env/options.hh"
#include "snn-core/file/is_regular.hh"
#include "snn-core/file/is_something.hh"
#include "snn-core/file/read.hh"
#include "snn-core/file/remove.hh"
#include "snn-core/file/write.hh"
#include "snn-core/file/dir/home.hh"
#include "snn-core/file/path/is_absolute.hh"
#include "snn-core/file/path/join.hh"
#include "snn-core/file/path/split.hh"
#include "snn-core/file/standard/error.hh"
#include "snn-core/file/standard/out.hh"
#include "snn-core/fmt/error_code.hh"
#include "snn-core/fmt/print.hh"
#include "snn-core/fn/common.hh"
#include "snn-core/map/sorted.hh"
#include "snn-core/map/unsorted.hh"
#include "snn-core/process/execute.hh"
#include "snn-core/process/spawner.hh"
#include "snn-core/random/number.hh"
#include "snn-core/range/step.hh"
#include "snn-core/range/view/element.hh"
#include "snn-core/range/view/enumerate.hh"
#include "snn-core/set/sorted.hh"
#include "snn-core/set/unsorted.hh"
#include "snn-core/string/range/split.hh"
#include "snn-core/string/range/wrap.hh"
#include "snn-core/utf8/is_valid.hh"
#include "build-tool/preprocessor.hh"
#include "build-tool/validator.hh"

namespace snn::app
{
    class generator final
    {
      public:
        generator() = default;

        // Non-copyable
        generator(const generator&)            = delete;
        generator& operator=(const generator&) = delete;

        // Non-movable
        generator(generator&&)            = delete;
        generator& operator=(generator&&) = delete;

        [[nodiscard]] bool add_application(str path)
        {
            if (verbose_level_ >= 3)
            {
                fmt::print_error_line("Adding application source: {}", path);
            }

            const auto [dir, base, ext] = file::path::split<cstrview>(path).value();

            if (ext != ".cc")
            {
                fmt::print_error_line("Error: Path must have \".cc\" extension: {}", path);
                return false;
            }

            constexpr cstrview path_comp_regexp = R"(\.?[A-Za-z]([A-Za-z0-9._-]*[A-Za-z0-9])?)";

            if (!validator::is_base(base))
            {
                fmt::print_error_line("Error: Unsupported character in basename: {}", base);
                fmt::print_error_line("Directories and filenames (excluding \".cc\" extension)"
                                      " must match the regular expression:\n{}",
                                      path_comp_regexp);
                return false;
            }

            if (!validator::is_directory(dir))
            {
                fmt::print_error_line("Error: Unsupported character in path: {}", dir);
                fmt::print_error_line("Directories and filenames (excluding \".cc\" extension)"
                                      " must match the regular expression:\n{}",
                                      path_comp_regexp);
                return false;
            }

            if (dir.has_front('/'))
            {
                fmt::print_error_line("Error: Path must be relative: {}", path);
                return false;
            }

            if (validator::is_reserved_target(dir, base))
            {
                fmt::print_error_line("Error: Reserved target: {}{}", dir, base);
                return false;
            }

            if (path.has_front('.') && !path.contains('/'))
            {
                fmt::print_error_line("Error: A path starting with a dot must include a slash: {}",
                                      path);
                return false;
            }

            if (file::is_regular(concat(path, ".ignore")))
            {
                fmt::print_error_line("Warning: Ignoring application source file: {}[.ignore]",
                                      path);
            }
            else if (!applications_.insert(path))
            {
                fmt::print_error_line("Error: Duplicate application source file: {}", path);
                return false;
            }

            return true;
        }

        [[nodiscard]] const auto& applications() const noexcept
        {
            return applications_;
        }

        [[nodiscard]] cstrview compiler_default() const noexcept
        {
            return compiler_default_;
        }

        [[nodiscard]] bool generate(const str& makefile, const str& makefile_depend) const
        {
            if (verbose_level_ >= 3)
            {
                fmt::print_error_line("Generating: {}", makefile);

                if (makefile_depend)
                {
                    fmt::print_error_line("Generating: {}", makefile_depend);
                }
            }

            strbuf mk{container::reserve, 1024};

            if (applications_.is_empty() || compiler_.is_empty())
            {
                fmt::print_error_line("Error: Nothing to generate");
                return false;
            }

            // Shared variables.

            mk << "CC = ";
            if (time_execution_)
            {
                mk << "time ";
            }
            mk << compiler_ << '\n';

            mk << "CFLAGS =";
            if (compiler_.has_front("clang"))
            {
                mk << " --config " << config_file_;
            }
            else
            {
                // GCC
                mk << " @" << config_file_;
            }

            if (optimize_)
            {
                mk << " -O2";
            }

            vec<str> cflags{container::reserve, 10};
            vec<str> phony_targets{container::reserve, 6};

            if (fuzz_)
            {
                cflags.append("-fsanitize=fuzzer,address,undefined,integer");
                cflags.append("-fno-sanitize-recover=all");
                cflags.append("-DFUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION");
            }
            else if (sanitize_)
            {
                cflags.append("-fsanitize=address,undefined,integer");
                cflags.append("-fno-sanitize-recover=all");
            }

            for (const cstrview macro : string::range::split{macros_, ','})
            {
                cflags.append(concat("-D", macro));
            }

            for (const auto& s : cflags)
            {
                mk << "\\\n\t\t " << s;
            }
            mk << '\n';

            if (include_path_)
            {
                mk << "INC = -iquote " << include_path_ << '\n';
            }
            else
            {
                mk << "INC = -iquote ./\n";
            }

            mk << "LINK = -L/usr/local/lib/\n";

#if defined(__FreeBSD__)
            if (makefile_depend)
            {
                mk << "\n.MAKE.DEPENDFILE=" << makefile_depend << '\n';
            }
#endif

            // Variables for each application.

            for (const auto [index, app] : applications_.range() | range::v::enumerate{})
            {
                str idx;
                idx << as_num(index);

                const auto executable = app.view_offset(0, -3); // Drop ".cc".

                mk << "\nAPP" << idx << " = " << executable << '\n';

                mk << "SRC" << idx << " = ";
                const auto sources = source_dependencies_(app);
                algo::join(sources.range(), "\\\n\t   ", mk, promise::no_overlap);
                mk << '\n';

                mk << "OBJ" << idx << " = $(SRC" << idx << ":.cc=.o)\n";

                mk << "LIB" << idx << " =";
                const auto libraries = library_dependencies_(app);
                for (const auto lib : libraries)
                {
                    mk << " -l" << lib;
                }
                mk << '\n';
            }

            // How to build object files (suffixes).

            mk << "\n";
            mk << "# Suffixes (how to build object files).\n";
            mk << "# First line deletes all previously specified suffixes.\n";
            mk << ".SUFFIXES:\n";
            mk << ".SUFFIXES: .cc .o\n";
            mk << ".cc.o:\n";
            mk << "\t$(CC) $(CFLAGS) $(INC) -c -o $@ $<\n";

            // Target: all

            phony_targets.append("all");
            mk << "\nall:";
            strbuf all{container::reserve, 8 * applications_.count()};
            for (const auto index : range::step<usize>{0, applications_.count()})
            {
                all << " $(APP" << as_num(index) << ')';
            }
            for (const auto [part, delim] : string::range::wrap{all, 90, " \\\n\t "})
            {
                mk << part << delim;
            }
            mk << '\n';

            for (const auto index : range::step<usize>{0, applications_.count()})
            {
                str idx;
                idx << as_num(index);
                mk << "\n$(APP" << idx << "): ${OBJ" << idx << "}\n";
                mk << "\t$(CC) $(CFLAGS) -o $(APP" << idx << ") $(OBJ" << idx << ")"
                   << " $(LINK) $(LIB" << idx << ")\n";
            }

            // Target: clean-executables

            phony_targets.append("clean-executables");
            mk << "\nclean-executables:\n";
            for (const auto index : range::step<usize>{0, applications_.count()})
            {
                mk << "\trm -f $(APP" << as_num(index) << ")\n";
            }

            // Target: clean-object-files

            phony_targets.append("clean-object-files");
            mk << "\nclean-object-files:\n";
            for (const auto index : range::step<usize>{0, applications_.count()})
            {
                mk << "\trm -f $(OBJ" << as_num(index) << ")\n";
            }

            // Target: clean

            phony_targets.append("clean");
            mk << "\nclean: clean-object-files clean-executables\n";

            if (!fuzz_)
            {
                // Target: destruct

                phony_targets.append("destruct");
                mk << "\ndestruct: clean\n";
                mk << "\trm -f " << makefile;
                if (makefile_depend)
                {
                    mk << ' ' << makefile_depend;
                }
                mk << '\n';

                // Target: run

                phony_targets.append("run");
                mk << "\nrun: all\n";
                for (const auto index : range::step<usize>{0, applications_.count()})
                {
                    mk << "\t./$(APP" << as_num(index) << ")\n";
                }
            }
            else
            {
                // Target: destruct

                phony_targets.append("destruct");
                mk << "\ndestruct: clean\n";
                mk << "\trm -f " << makefile;
                if (makefile_depend)
                {
                    mk << ' ' << makefile_depend;
                }
                mk << '\n';
                for (const auto index : range::step<usize>{0, applications_.count()})
                {
                    mk << "\trm -rf $(APP" << as_num(index) << ").corpus\n";
                }

                // Target: minimize-corpus
                // Target: compress-corpus
                // Target: run

                const usize size_guess = 256 * applications_.count();
                strbuf minimize{container::reserve, size_guess};
                strbuf compress{container::reserve, size_guess};
                strbuf run{container::reserve, size_guess};

                str cd_dir_and;

                for (const auto& app : applications_)
                {
                    const auto [dir, base, ext] = file::path::split<cstrview>(app).value();

                    cd_dir_and.clear();
                    if (dir)
                    {
                        cd_dir_and << "cd " << dir << " && ";
                    }

                    // minimize-corpus

                    minimize << "\t@test ! -e " << dir << base << ".corpus.old || \\\n"
                             << "\t\t(echo 'Error: Directory exists: " << dir << base
                             << ".corpus.old'; exit 1;)\n";
                    minimize << '\t' << "mv " << dir << base << ".corpus " << dir << base
                             << ".corpus.old\n";
                    minimize << "\tmkdir " << dir << base << ".corpus\n";
                    minimize << '\t' << cd_dir_and << "./" << base << " -merge=1 " << base
                             << ".corpus " << base << ".corpus.old\n";
                    minimize << "\trm -rf " << dir << base << ".corpus.old\n";

                    // compress-corpus

#if defined(__FreeBSD__)
                    constexpr cstrview tarcmd{"tar -cz --gid 0 --uid 0 -f "};
#elif defined(__linux__)
                    constexpr cstrview tarcmd{"tar -cz --owner=0 --group=0 -f "};
#else
                    constexpr cstrview tarcmd{"tar -czf "};
#endif

                    compress << "\trm -f " << dir << base << ".corpus.tar.gz\n";
                    compress << '\t' << cd_dir_and << tarcmd << base << ".corpus.tar.gz " << base
                             << ".corpus\n";
                    compress << "\trm -rf " << dir << base << ".corpus\n";

                    // run

                    run << "\t@test -d " << dir << base << ".corpus || test ! -e " << dir << base
                        << ".corpus.tar.gz || \\\n";
                    run << "\t\t(echo '" << cd_dir_and << "tar -xzf " << base
                        << ".corpus.tar.gz' && \\\n";
                    run << "\t\t" << cd_dir_and << "tar -xzf " << base << ".corpus.tar.gz)\n";
                    run << "\t@test -d " << dir << base << ".corpus || \\\n";
                    run << "\t\t(echo 'mkdir " << dir << base << ".corpus' && mkdir " << dir << base
                        << ".corpus)\n";
                    run << '\t' << cd_dir_and << "./" << base << " -rss_limit_mb=3072 -timeout=5";
                    if (applications_.count() > 1)
                    {
                        run << " -max_total_time=900"; // Seconds
                    }
                    run << " " << base << ".corpus/\n";
                }

                phony_targets.append("minimize-corpus");
                phony_targets.append("compress-corpus");
                phony_targets.append("run");

                mk << "\nminimize-corpus: all\n" << minimize;
                mk << "\ncompress-corpus: minimize-corpus\n" << compress;
                mk << "\nrun: all\n" << run;
            }

            // Phony targets.
            mk << "\n.PHONY:";
            for (const auto& s : phony_targets)
            {
                mk << ' ' << s;
            }
            mk << '\n';

#if !defined(__FreeBSD__)
            if (makefile_depend)
            {
                mk << "\n-include " << makefile_depend << '\n';
            }
#endif

            if (!file::write(makefile, mk, file::option::create_or_fail))
            {
                fmt::print_error_line("Error: Failed to create: {}", makefile);
                return false;
            }

            if (makefile_depend)
            {
                const strbuf dependency_list = dependency_list_();
                if (!file::write(makefile_depend, dependency_list))
                {
                    fmt::print_error_line("Error: Failed to write to: {}", makefile_depend);
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] bool parse()
        {
            for (const str& source : applications_)
            {
                if (verbose_level_ >= 3)
                {
                    fmt::print_error_line("Parsing: {}", source);
                }

                constexpr u32 depth = 0;
                if (!parse_recursive_(source, depth))
                {
                    return false;
                }
            }
            return true;
        }

        [[nodiscard]] bool setup_compiler_and_macros(const cstrview compiler, const cstrview macros)
        {
            if (setup_compiler_(compiler) && set_macros_(macros))
            {
                if (verbose_level_ >= 3)
                {
                    print_predefined_macros_();
                    print_compiler_include_paths_();
                }

                return true;
            }
            return false;
        }

        void set_fuzz(const bool b) noexcept
        {
            fuzz_ = b;
        }

        void set_optimize(const bool b) noexcept
        {
            optimize_ = b;
        }

        void set_sanitize(const bool b) noexcept
        {
            sanitize_ = b;
        }

        void set_time_execution(const bool b) noexcept
        {
            time_execution_ = b;
        }

        void set_verbose_level(const u32 i) noexcept
        {
            verbose_level_ = i;
        }

      private:
        struct dependencies
        {
            set::unsorted<str> libraries;
            set::unsorted<str> source_files;
            set::unsorted<str> header_files;
        };

        map::unsorted<str, dependencies> dependencies_;
        map::sorted<str, str> predefined_macros_;

        set::sorted<str> applications_;

        vec<str> compiler_include_paths_;

        str config_file_;
        str include_path_;

        cstrview compiler_;
        cstrview compiler_default_{"clang++"};
        cstrview macros_;

        u32 verbose_level_ = 0;

        bool fuzz_           = false;
        bool optimize_       = false;
        bool sanitize_       = false;
        bool time_execution_ = false;

        [[nodiscard]] bool ask_compiler_for_defaults_()
        {
            snn_should(compiler_);

            process::command cmd;

            cmd.append_command(compiler_, promise::is_valid);

            if (compiler_.has_front("clang"))
            {
                cmd << " --config ";
            }
            else
            {
                cmd << " @";
            }
            cmd.append_command(config_file_, promise::is_valid);

            if (optimize_)
            {
                cmd << " -O2";
            }
            cmd << " -v -x c++ /dev/null -dM -E 2>&1";

            if (verbose_level_ >= 2)
            {
                fmt::print_error_line("{}", cmd.to<cstrview>());
            }

            constexpr cstrview include_list_start{"#include <...> search starts here:"};

            enum parse_state : u8
            {
                include_list,
                maybe_define,
            };

            parse_state state = maybe_define;

            auto output = process::execute_and_consume_output(cmd);
            if (output)
            {
                while (const auto line = output.read_line<cstrview>())
                {
                    auto rng = line.value(promise::has_value).range();

                    rng.pop_front_while(chr::is_ascii_control_or_space);
                    rng.pop_back_while(chr::is_ascii_control_or_space);

                    switch (state)
                    {
                        case maybe_define:
                            if (rng.drop_front("#define "))
                            {
                                str macro{rng.pop_front_while(fn::is{fn::not_equal_to{}, ' '})};

                                if (macro)
                                {
                                    rng.drop_front(' ');
                                    str value{rng};

                                    predefined_macros_.insert_or_assign(std::move(macro),
                                                                        std::move(value));
                                }
                            }
                            else
                            {
                                const cstrview trimmed_line{rng};
                                if (trimmed_line == include_list_start)
                                {
                                    state = include_list;
                                }
                            }

                            break;

                        case include_list:
                            if (rng.has_front('/'))
                            {
                                str path{rng};
                                if (!path.has_back('/'))
                                {
                                    path.append('/');
                                }
                                compiler_include_paths_.append(std::move(path));
                            }
                            else
                            {
                                state = maybe_define;
                            }

                            break;
                    }
                }

                if (predefined_macros_ && compiler_include_paths_)
                {
                    return output.exit_status() == constant::exit::success;
                }
            }

            return false;
        }

        [[nodiscard]] cstrview compiler_config_name_() const noexcept
        {
            if (compiler_.has_front("clang"))
            {
                return cstrview{".clang"};
            }
            return cstrview{".gcc"};
        }

        [[nodiscard]] strbuf dependency_list_() const
        {
            strbuf dependency_list{container::reserve, 4096};

            for (const auto& file : dependencies_.range() | range::v::element<0>{})
            {
                if (file.has_back(".cc"))
                {
                    str obj{file};
                    obj.drop_back_n(string_size(".cc"));
                    obj.append(".o");

                    dependency_list << obj << ": " << file;

                    const auto headers = header_dependencies_(file);
                    for (const auto header : headers)
                    {
                        dependency_list << " " << header;
                    }

                    dependency_list << '\n';
                }
            }

            strbuf wrapped{container::reserve, dependency_list.size()};
            for (const auto [part, delim] : string::range::wrap{dependency_list, 90, " \\\n  "})
            {
                wrapped << part << delim;
            }
            return wrapped;
        }

        [[nodiscard]] bool detect_include_path_(const cstrview file)
        {
            if (file::path::is_absolute(file))
            {
                return false;
            }

            str check;

            // Current directory.

            include_path_ = "./";

            check << include_path_ << file;
            if (file::is_regular(check))
            {
                return true;
            }

            // Parent directory/directories.

            include_path_ = "../";
            int levels    = 1;
            do
            {
                check.clear();
                check << include_path_ << file;
                if (file::is_regular(check))
                {
                    return true;
                }

                include_path_ << "../";
                ++levels;

            } while (levels < 10); // Arbitrary

            // $HOME/project/cpp/

            include_path_ = file::dir::home<cstrview>().value_or_default();
            if (include_path_)
            {
                include_path_ = file::path::join(include_path_, "project/cpp/");

                check.clear();
                check << include_path_ << file;

                if (validator::is_file_path(check))
                {
                    return file::is_regular(check);
                }
            }

            return false;
        }

        [[nodiscard]] bool find_compiler_config_()
        {
            // Always include a directory separator in the path, even if the config file is in the
            // current directory, otherwise clang will look for the file elsewhere:
            // https://clang.llvm.org/docs/UsersManual.html#configuration-files

            const cstrview name = compiler_config_name_();

            str path = "./";

            config_file_.clear();
            config_file_ << path << name;
            if (file::is_regular(config_file_))
            {
                return true;
            }

            int levels = 1;
            path       = "../";
            do
            {
                config_file_.clear();
                config_file_ << path << name;
                if (file::is_regular(config_file_))
                {
                    return true;
                }

                path << "../";
                ++levels;

            } while (levels < 10); // Arbitrary

            return false;
        }

        [[nodiscard]] set::unsorted<cstrview> header_dependencies_(const str& file) const
        {
            set::unsorted<cstrview> dependencies;
            header_dependencies_recursive_(file, dependencies);
            return dependencies;
        }

        void header_dependencies_recursive_(const str& file,
                                            set::unsorted<cstrview>& dependencies) const
        {
            const auto& file_deps = dependencies_.get(file).value();
            for (const str& header_file : file_deps.header_files)
            {
                if (dependencies.insert(header_file.view()))
                {
                    header_dependencies_recursive_(header_file, dependencies);
                }
            }
        }

        [[nodiscard]] set::unsorted<cstrview> library_dependencies_(const str& source_file) const
        {
            set::unsorted<cstrview> dependencies;
            set::unsorted<cstrview> handled; // In case there is a circular dependency.
            library_dependencies_recursive_(source_file, dependencies, handled);
            return dependencies;
        }

        void library_dependencies_recursive_(const str& file, set::unsorted<cstrview>& dependencies,
                                             set::unsorted<cstrview>& handled) const
        {
            const auto& file_deps = dependencies_.get(file).value();

            for (const str& library : file_deps.libraries)
            {
                dependencies.insert(library.view());
            }

            for (const str& source_file : file_deps.source_files)
            {
                if (handled.insert(source_file.view()))
                {
                    library_dependencies_recursive_(source_file, dependencies, handled);
                }
            }

            for (const str& header_file : file_deps.header_files)
            {
                if (handled.insert(header_file.view()))
                {
                    library_dependencies_recursive_(header_file, dependencies, handled);
                }
            }
        }

        [[nodiscard]] static bool parse_libraries_(const cstrview line,
                                                   set::unsorted<str>& libraries)
        {
            const usize pos = line.find('[').value_or_npos();
            if (pos != constant::npos)
            {
                for (cstrview word : string::range::split{line.view(pos), ' '})
                {
                    if (word.has_front("[#lib:") && word.has_back(']'))
                    {
                        word.drop_front_n(string_size("[#lib:"));
                        word.drop_back_n(string_size("]"));

                        if (validator::is_library(word))
                        {
                            libraries.insert(word);
                        }
                        else
                        {
                            fmt::print_error_line("Error: Invalid library name: {}", word);
                            return false;
                        }
                    }
                }
            }
            return true;
        }

        [[nodiscard]] bool parse_recursive_(const str& file, const u32 depth)
        {
            constexpr u32 max_depth = 128;      // Arbitrary (around 10 is normal for `snn-core`).
            if (depth > max_depth) [[unlikely]] // Clang bug if unreachable code warning.
            {
                fmt::print_error_line("Error: Maximum recursion depth ({}) exceeded", max_depth);
                return false;
            }

            auto ins_res = dependencies_.insert_inplace(file);
            if (!ins_res.was_inserted())
            {
                // Already parsed.
                return true;
            }
            auto& deps = ins_res.value();

            strbuf contents;
            if (file::read(file, contents) && contents)
            {
                if (!utf8::is_valid(contents))
                {
                    fmt::print_error("Warning: File does not pass UTF-8 validation:\n"
                                     "         {}\n",
                                     file);
                }

                app::preprocessor preprocessor{predefined_macros_, compiler_include_paths_};

                str file_next;
                for (cstrview line : string::range::split{contents, '\n'})
                {
                    ascii::trim_inplace(line);

                    const auto status = preprocessor.process(line);
                    if (status != preprocessor.compile)
                    {
                        if (status == preprocessor.not_understood && line.has_front("#include "))
                        {
                            fmt::print_error("Warning: Ignoring #include directive in #if that is"
                                             " not understood:\n"
                                             "         {}\n"
                                             "         {}\n",
                                             line, file);
                        }

                        if (line.is_empty() || line.has_front('#') || line.has_front("//"))
                        {
                            continue;
                        }
                        else
                        {
                            break;
                        }
                    }

                    if (line.has_front("#include \""))
                    {
                        if (!parse_libraries_(line, deps.libraries))
                        {
                            fmt::print_error_line("Error: Parsing failed while parsing: {}", file);
                            return false;
                        }

                        line.drop_front_n(string_size("#include \""));

                        const usize pos = line.find(R"(.hh")").value_or_npos();
                        if (pos != constant::npos)
                        {
                            line.truncate(pos + string_size(".hh"));

                            if (!validator::is_file_path(line))
                            {
                                fmt::print_error_line("Error: Invalid file path: {}", line);
                                return false;
                            }

                            if (include_path_.is_empty())
                            {
                                if (!detect_include_path_(line))
                                {
                                    fmt::print_error_line(
                                        "Error: Failed to detect include path from: {}", line);
                                    return false;
                                }
                            }

                            file_next.clear();
                            file_next << include_path_ << line;

                            if (deps.header_files.insert(file_next))
                            {
                                if (!parse_recursive_(file_next, depth + 1))
                                {
                                    fmt::print_error_line("Error: Parsing failed while parsing: {}",
                                                          file);
                                    return false;
                                }

                                file_next.drop_back_n(string_size("hh"));
                                file_next.append("cc");
                                if (!deps.source_files.contains(file_next) &&
                                    file::is_regular(file_next))
                                {
                                    deps.source_files.insert(file_next);
                                    if (!parse_recursive_(file_next, depth + 1))
                                    {
                                        fmt::print_error_line(
                                            "Error: Parsing failed while parsing: {}", file);
                                        return false;
                                    }
                                }
                            }
                        }

                        continue;
                    }
                    else if (line.has_front("#include <"))
                    {
                        if (!parse_libraries_(line, deps.libraries))
                        {
                            fmt::print_error_line("Error: Parsing failed while parsing: {}", file);
                            return false;
                        }

                        continue;
                    }
                    else if (line.is_empty() || line.has_front('#') || line.has_front("//"))
                    {
                        continue;
                    }

                    break;
                }

                return true;
            }

            fmt::print_error_line("Error: File is empty/unreadable: {}", file);

            return false;
        }

        void print_compiler_include_paths_() const
        {
            strbuf out{container::reserve, constant::size::kibibyte<usize>};
            out.append("Include paths (from compiler):\n");
            for (const auto& path : compiler_include_paths_)
            {
                fmt::format_append(" {}\n", out, promise::no_overlap, path);
            }
            out.append("End of include paths.\n");
            file::standard::out{} << out;
        }

        void print_predefined_macros_() const
        {
            strbuf out{container::reserve, 16 * constant::size::kibibyte<usize>};
            out.append("Predefined macros (from compiler and command line):\n");
            for (const auto& p : predefined_macros_)
            {
                fmt::format_append(" #define {} {}\n", out, promise::no_overlap, p.first, p.second);
            }
            out.append("End of predefined macros.\n");
            file::standard::out{} << out;
        }

        [[nodiscard]] bool setup_compiler_(const cstrview compiler)
        {
            compiler_ = compiler;

            if (compiler_.is_empty())
            {
                compiler_ = compiler_default_;
            }

            if (!validator::is_compiler(compiler_))
            {
                fmt::print_error_line("Error: Invalid compiler: {}", compiler_);
                fmt::print_error_line("The compiler must match the regular expression:\n{}",
                                      R"((clang|g)\+\+(-devel|[0-9]{0,2}))");
                return false;
            }

            if (!find_compiler_config_())
            {
                const cstrview name = compiler_config_name_();
                fmt::print_error_line("Error: \"{}\" config not found in current directory"
                                      " or in any parent directory",
                                      name);
                return false;
            }

            if (!ask_compiler_for_defaults_())
            {
                fmt::print_error_line("Error: Could not get predefined macros"
                                      " and include paths from compiler");
                return false;
            }

            return true;
        }

        [[nodiscard]] bool set_macros_(const cstrview macros)
        {
            macros_ = macros;
            ascii::trim_right_inplace(macros_, ',');
            for (const cstrview macro : string::range::split{macros_, ','})
            {
                if (!validator::is_macro(macro))
                {
                    fmt::print_error_line("Error: Invalid macro: {}", macro);
                    return false;
                }

                if (verbose_level_ >= 3)
                {
                    fmt::print_error_line("Adding macro: #define {} 1", macro);
                }

                predefined_macros_.insert_or_assign(macro, "1");
            }
            return true;
        }

        [[nodiscard]] set::unsorted<cstrview> source_dependencies_(const str& source_file) const
        {
            set::unsorted<cstrview> dependencies;
            dependencies.insert(source_file.view());
            set::unsorted<cstrview> handled; // In case there is a circular dependency.
            source_dependencies_recursive_(source_file, dependencies, handled);
            return dependencies;
        }

        void source_dependencies_recursive_(const str& file, set::unsorted<cstrview>& dependencies,
                                            set::unsorted<cstrview>& handled) const
        {
            const auto& file_deps = dependencies_.get(file).value();

            for (const str& source_file : file_deps.source_files)
            {
                if (dependencies.insert(source_file.view()))
                {
                    source_dependencies_recursive_(source_file, dependencies, handled);
                }
            }

            for (const str& header_file : file_deps.header_files)
            {
                if (handled.insert(header_file.view()))
                {
                    source_dependencies_recursive_(header_file, dependencies, handled);
                }
            }
        }
    };

    namespace
    {
        int spawn(const str& path, vec<str> arguments)
        {
            auto spawn_res = process::spawner{path, std::move(arguments)}.spawn();
            if (spawn_res)
            {
                const process::termination_status term = spawn_res.value().wait().value();
                if (term.with_exit_status())
                {
                    return term.exit_status();
                }
                else
                {
                    fmt::print_error_line("Error: Exited abnormally: {}", path);
                }
            }
            else
            {
                fmt::print_error_line("Error: Failed to execute: {}", path);
                fmt::print_error_line("Error: {}", spawn_res.error_code());
            }

            return constant::exit::failure;
        }

        int make(const str& makefile, str target, const u32 verbose_level)
        {
            if (verbose_level >= 2)
            {
                fmt::print_error_line("make -f {} {}", makefile, target);
            }

            vec<str> spawn_args{container::reserve, 4};

            if (verbose_level == 0 || (verbose_level == 1 && target.has_front("clean")))
            {
                spawn_args.append("-s"); // "Do not echo any commands as they are executed."
            }
            spawn_args.append("-f");
            spawn_args.append(makefile);
            spawn_args.append(std::move(target));

            return app::spawn("make", std::move(spawn_args));
        }

        [[nodiscard]] str temporary_makefile_name()
        {
            for (loop::count lc{10}; lc--;) // X tries.
            {
                str name = "tmp-";

                const auto n                = random::number<u32>();
                constexpr usize pad_to_size = sizeof(n) * 2;
                name.append_integral<math::base::hex>(n, pad_to_size);

                name.append(".mk");

                if (!file::is_something(name))
                {
                    return name;
                }
            }

            // This should never happen, u32 has over 4 billion unique values.
            throw_or_abort("Failed to generate unique makefile name");
        }

        int build(const cstrview program_name, const array_view<const env::argument> arguments)
        {
            env::options opts{arguments,
                              {
                                  {"compiler", 'c', env::option::takes_values},
                                  {"define", 'd', env::option::takes_values},
                                  {"optimize", 'o'},
                                  {"sanitize", 's'},
                                  {"time-execution", 't'},
                                  {"verbose", 'v'},
                              },
                              promise::is_sorted};

            if (!opts)
            {
                fmt::print_error_line("Error: {}.", opts.error_message());
                return constant::exit::failure;
            }

            app::generator gen;

            const auto args = opts.arguments();
            if (args.count() >= 1)
            {
                const bool optimize       = opts.option('o').is_set();
                const bool sanitize       = opts.option('s').is_set();
                const bool time_execution = opts.option('t').is_set();
                auto verbose_level        = opts.option('v').count();

                if (time_execution)
                {
                    verbose_level = math::max(verbose_level, 1);
                }

                gen.set_optimize(optimize);
                gen.set_sanitize(sanitize);
                gen.set_time_execution(time_execution);
                gen.set_verbose_level(verbose_level);

                // Makefile

                const str makefile = app::temporary_makefile_name();

                // Compiler & macros.

                const cstrview compiler = opts.option('c').values().back().value_or_default();
                const cstrview macros   = opts.option('d').values().back().value_or_default();
                if (!gen.setup_compiler_and_macros(compiler, macros))
                {
                    return constant::exit::failure;
                }

                // Sources

                for (const auto arg : args)
                {
                    if (!gen.add_application(arg.to<str>()))
                    {
                        return constant::exit::failure;
                    }
                }

                if (gen.applications().is_empty())
                {
                    fmt::print_error_line("Error: No application source files to process");
                    return constant::exit::failure;
                }

                // Parse, generate & build.

                if (gen.parse())
                {
                    const str makefile_depend; // Empty (don't generate).

                    if (gen.generate(makefile, makefile_depend))
                    {
                        app::make(makefile, "clean", verbose_level);

                        const int exit_status = app::make(makefile, "all", verbose_level);

                        app::make(makefile, "clean-object-files", verbose_level);

                        if (verbose_level >= 3)
                        {
                            fmt::print_error_line("Deleting: {}", makefile);
                        }
                        file::remove(makefile).or_throw();

                        return exit_status;
                    }
                }
            }
            else
            {
                strbuf usage{container::reserve, 600};

                usage << "Usage: " << program_name << " build [options] [--] app.cc [...]\n";

                usage << '\n';

                usage << "Options:\n";
                usage << "-o --optimize            Optimize (-O2)\n";
                usage << "-t --time-execution      Time command execution (implies verbose)\n";
                usage << "-s --sanitize            Enable sanitizers (Address & "
                         "UndefinedBehavior)\n";
                usage << "-c --compiler compiler   Compiler (default: " << gen.compiler_default()
                      << ")\n";
                usage << "-d --define MACRO[,...]  Define macro(s)\n";
                usage << "-v --verbose             Increase verbosity (up to three times)\n";

                usage << '\n';

                usage << "Verbosity levels:\n";
                usage << "1. Show compile/run commands\n";
                usage << "2. Show all commands\n";
                usage << "3. Debug\n";

                file::standard::error{} << usage;
            }

            return constant::exit::failure;
        }

        int gen(const cstrview program_name, const array_view<const env::argument> arguments)
        {
            env::options opts{arguments,
                              {
                                  {"compiler", 'c', env::option::takes_values},
                                  {"define", 'd', env::option::takes_values},
                                  {"fuzz", 'z'},
                                  {"makefile", 'f', env::option::takes_values},
                                  {"optimize", 'o'},
                                  {"sanitize", 's'},
                                  {"time-execution", 't'},
                                  {"verbose", 'v'},
                              },
                              promise::is_sorted};

            if (!opts)
            {
                fmt::print_error_line("Error: {}", opts.error_message());
                return constant::exit::failure;
            }

            app::generator gen;

            const auto args = opts.arguments();
            if (args.count() >= 1)
            {
                const bool fuzz           = opts.option('z').is_set();
                const bool optimize       = opts.option('o').is_set();
                const bool sanitize       = opts.option('s').is_set();
                const bool time_execution = opts.option('t').is_set();
                auto verbose_level        = opts.option('v').count();

                if (time_execution)
                {
                    verbose_level = math::max(verbose_level, 1);
                }

                gen.set_fuzz(fuzz);
                gen.set_optimize(optimize);
                gen.set_sanitize(sanitize);
                gen.set_time_execution(time_execution);
                gen.set_verbose_level(verbose_level);

                // Makefile

                str makefile;

                if (auto opt = opts.option('f'); opt.is_set())
                {
                    makefile = opt.values().back().value_or_default();
                }
                else
                {
                    makefile = "makefile";
                }

                if (!app::validator::is_file_path(makefile))
                {
                    fmt::print_error_line("Error: Invalid makefile name: {}", makefile);
                    return constant::exit::failure;
                }

                if (file::is_something(makefile))
                {
                    fmt::print_error_line("Error: Makefile already exists: {}", makefile);
                    return constant::exit::failure;
                }

                // Compiler & macros.

                const cstrview compiler = opts.option('c').values().back().value_or_default();
                const cstrview macros   = opts.option('d').values().back().value_or_default();
                if (!gen.setup_compiler_and_macros(compiler, macros))
                {
                    return constant::exit::failure;
                }

                // Sources

                for (const auto arg : args)
                {
                    if (!gen.add_application(arg.to<str>()))
                    {
                        return constant::exit::failure;
                    }
                }

                if (gen.applications().is_empty())
                {
                    fmt::print_error_line("Error: No application source files to process");
                    return constant::exit::failure;
                }

                // Parse & generate.

                if (gen.parse())
                {
                    const str makefile_depend = concat(makefile, ".depend");

                    if (gen.generate(makefile, makefile_depend))
                    {
                        return constant::exit::success;
                    }
                }
            }
            else
            {
                strbuf usage{container::reserve, 600};

                usage << "Usage: " << program_name << " gen [options] [--] app.cc [...]\n";

                usage << '\n';

                usage << "Options:\n";
                usage << "-f --makefile file       Write to file instead of \"makefile\"\n";
                usage << "-o --optimize            Optimize (-O2)\n";
                usage << "-t --time-execution      Time command execution (implies verbose)\n";
                usage << "-s --sanitize            Enable sanitizers (Address & "
                         "UndefinedBehavior)\n";
                usage << "-z --fuzz                Build libFuzzer binary (implies sanitizers)\n";
                usage << "-c --compiler compiler   Compiler (default: " << gen.compiler_default()
                      << ")\n";
                usage << "-d --define MACRO[,...]  Define macro(s)\n";
                usage << "-v --verbose             Increase verbosity (up to three times)\n";

                usage << '\n';

                usage << "Verbosity levels:\n";
                usage << "1. Show compile/run commands\n";
                usage << "2. Show all commands\n";
                usage << "3. Debug\n";

                file::standard::error{} << usage;
            }

            return constant::exit::failure;
        }

        int run(const cstrview program_name, const array_view<const env::argument> arguments)
        {
            env::options opts{arguments,
                              {
                                  {"compiler", 'c', env::option::takes_values},
                                  {"define", 'd', env::option::takes_values},
                                  {"optimize", 'o'},
                                  {"sanitize", 's'},
                                  {"time-execution", 't'},
                                  {"verbose", 'v'},
                              },
                              promise::is_sorted};

            if (!opts)
            {
                fmt::print_error_line("Error: {}", opts.error_message());
                return constant::exit::failure;
            }

            app::generator gen;

            auto args = opts.arguments();
            if (args.count() >= 1)
            {
                const bool optimize       = opts.option('o').is_set();
                const bool sanitize       = opts.option('s').is_set();
                const bool time_execution = opts.option('t').is_set();
                auto verbose_level        = opts.option('v').count();

                if (time_execution)
                {
                    verbose_level = math::max(verbose_level, 1);
                }

                gen.set_optimize(optimize);
                gen.set_sanitize(sanitize);
                gen.set_time_execution(time_execution);
                gen.set_verbose_level(verbose_level);

                // Makefile

                const str makefile = app::temporary_makefile_name();

                // Compiler & macros.

                const cstrview compiler = opts.option('c').values().back().value_or_default();
                const cstrview macros   = opts.option('d').values().back().value_or_default();
                if (!gen.setup_compiler_and_macros(compiler, macros))
                {
                    return constant::exit::failure;
                }

                // Source

                auto app_src         = args.front().value().to<str>();
                const str spawn_path = concat("./", app_src.view_offset(0, -3)); // Drop ".cc".

                if (!gen.add_application(std::move(app_src)))
                {
                    return constant::exit::failure;
                }

                args.drop_front_n(1);

                // Make sure that the application wasn't ignored.
                if (gen.applications().is_empty())
                {
                    fmt::print_error_line("Error: No application source files to process");
                    return constant::exit::failure;
                }

                // Parse, generate & run.

                if (gen.parse())
                {
                    const str makefile_depend; // Empty (don't generate).

                    if (gen.generate(makefile, makefile_depend))
                    {
                        app::make(makefile, "clean", verbose_level);

                        int exit_status = app::make(makefile, "all", verbose_level);

                        if (exit_status == constant::exit::success)
                        {
                            vec<str> spawn_args{container::reserve, args.count()};

                            for (const auto& arg : args)
                            {
                                spawn_args.append(arg.to<str>());
                            }

                            if (verbose_level >= 1)
                            {
                                if (spawn_args)
                                {
                                    fmt::print_error_line("{} ...", spawn_path);
                                }
                                else
                                {
                                    fmt::print_error_line("{}", spawn_path);
                                }
                            }

                            exit_status = app::spawn(spawn_path, std::move(spawn_args));
                        }

                        app::make(makefile, "clean", verbose_level);

                        if (verbose_level >= 3)
                        {
                            fmt::print_error_line("Deleting: {}", makefile);
                        }
                        file::remove(makefile).or_throw();

                        return exit_status;
                    }
                }
            }
            else
            {
                strbuf usage{container::reserve, 600};

                usage << "Usage: " << program_name << " run [options] [--] app.cc [arguments]\n";

                usage << '\n';

                usage << "Options:\n";
                usage << "-o --optimize            Optimize (-O2)\n";
                usage << "-t --time-execution      Time command execution (implies verbose)\n";
                usage << "-s --sanitize            Enable sanitizers (Address & "
                         "UndefinedBehavior)\n";
                usage << "-c --compiler compiler   Compiler (default: " << gen.compiler_default()
                      << ")\n";
                usage << "-d --define MACRO[,...]  Define macro(s)\n";
                usage << "-v --verbose             Increase verbosity (up to three times)\n";

                usage << '\n';

                usage << "Verbosity levels:\n";
                usage << "1. Show compile/run commands\n";
                usage << "2. Show all commands\n";
                usage << "3. Debug\n";

                file::standard::error{} << usage;
            }

            return constant::exit::failure;
        }

        int runall(const cstrview program_name, const array_view<const env::argument> arguments)
        {
            env::options opts{arguments,
                              {
                                  {"compiler", 'c', env::option::takes_values},
                                  {"define", 'd', env::option::takes_values},
                                  {"optimize", 'o'},
                                  {"sanitize", 's'},
                                  {"time-execution", 't'},
                                  {"verbose", 'v'},
                              },
                              promise::is_sorted};

            if (!opts)
            {
                fmt::print_error_line("Error: {}", opts.error_message());
                return constant::exit::failure;
            }

            app::generator gen;

            const auto args = opts.arguments();
            if (args.count() >= 1)
            {
                const bool optimize       = opts.option('o').is_set();
                const bool sanitize       = opts.option('s').is_set();
                const bool time_execution = opts.option('t').is_set();
                auto verbose_level        = opts.option('v').count();

                if (time_execution)
                {
                    verbose_level = math::max(verbose_level, 1);
                }

                gen.set_optimize(optimize);
                gen.set_sanitize(sanitize);
                gen.set_time_execution(time_execution);
                gen.set_verbose_level(verbose_level);

                // Makefile

                const str makefile = app::temporary_makefile_name();

                // Compiler & macros.

                const cstrview compiler = opts.option('c').values().back().value_or_default();
                const cstrview macros   = opts.option('d').values().back().value_or_default();
                if (!gen.setup_compiler_and_macros(compiler, macros))
                {
                    return constant::exit::failure;
                }

                // Sources

                for (const auto arg : args)
                {
                    if (!gen.add_application(arg.to<str>()))
                    {
                        return constant::exit::failure;
                    }
                }

                if (gen.applications().is_empty())
                {
                    fmt::print_error_line("Error: No application source files to process");
                    return constant::exit::failure;
                }

                // Parse, generate & run.

                if (gen.parse())
                {
                    const str makefile_depend; // Empty (don't generate).

                    if (gen.generate(makefile, makefile_depend))
                    {
                        app::make(makefile, "clean", verbose_level);

                        const int exit_status = app::make(makefile, "run", verbose_level);

                        app::make(makefile, "clean", verbose_level);

                        if (verbose_level >= 3)
                        {
                            fmt::print_error_line("Deleting: {}", makefile);
                        }
                        file::remove(makefile).or_throw();

                        return exit_status;
                    }
                }
            }
            else
            {
                strbuf usage{container::reserve, 600};

                usage << "Usage: " << program_name << " runall [options] [--] app.cc [...]\n";

                usage << '\n';

                usage << "Options:\n";
                usage << "-o --optimize            Optimize (-O2)\n";
                usage << "-t --time-execution      Time command execution (implies verbose)\n";
                usage << "-s --sanitize            Enable sanitizers (Address & "
                         "UndefinedBehavior)\n";
                usage << "-c --compiler compiler   Compiler (default: " << gen.compiler_default()
                      << ")\n";
                usage << "-d --define MACRO[,...]  Define macro(s)\n";
                usage << "-v --verbose             Increase verbosity (up to three times)\n";

                usage << '\n';

                usage << "Verbosity levels:\n";
                usage << "1. Show compile/run commands\n";
                usage << "2. Show all commands\n";
                usage << "3. Debug\n";

                file::standard::error{} << usage;
            }

            return constant::exit::failure;
        }
    }
}

namespace snn
{
    int main(array_view<const env::argument> arguments)
    {
        const auto program_name = arguments.front().value_or_default().to<cstrview>();
        arguments.drop_front_n(1);

        if (arguments)
        {
            const auto command = arguments.front().value().to<cstrview>();

            if (command == "build")
            {
                return app::build(program_name, arguments);
            }

            if (command == "gen")
            {
                return app::gen(program_name, arguments);
            }

            if (command == "run")
            {
                return app::run(program_name, arguments);
            }

            if (command == "runall")
            {
                return app::runall(program_name, arguments);
            }
        }

        strbuf usage{container::reserve, 300};

        usage << "Usage: " << program_name << " <command> [arguments]\n";

        usage << "\n";

        usage << "Commands:\n";
        usage << "build   Build one or more applications\n";
        usage << "gen     Generate a makefile for one or more applications\n";
        usage << "run     Build and run a single application with optional arguments\n";
        usage << "runall  Build and run one or more applications\n";

        usage << "\n";

        usage << "For more information run a command without arguments, e.g.:\n";
        usage << program_name << " build\n";

        file::standard::error{} << usage;

        return constant::exit::failure;
    }
}
