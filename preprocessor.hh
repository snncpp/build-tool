// Copyright (c) 2022 Mikael Simonsson <https://mikaelsimonsson.com>.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "snn-core/strcore.hh"
#include "snn-core/vec.hh"
#include "snn-core/chr/common.hh"
#include "snn-core/file/is_regular.hh"
#include "snn-core/fn/common.hh"
#include "snn-core/map/sorted.hh"
#include "snn-core/pair/common.hh"
#include "build-tool/validator.hh"

namespace snn::app
{
    class preprocessor final
    {
      public:
        enum status : u8
        {
            compile,
            skip,
            not_understood,
        };

        explicit preprocessor(const map::sorted<str, str>& predefined_macros,
                              const vec<str>& include_paths) noexcept
            : predefined_macros_{predefined_macros},
              include_paths_{include_paths}
        {
        }

        preprocessor(const map::sorted<str, str>&&, const vec<str>&&) = delete;
        preprocessor(const map::sorted<str, str>&&, const vec<str>&)  = delete;
        preprocessor(const map::sorted<str, str>&, const vec<str>&&)  = delete;

        // Non-copyable
        preprocessor(const preprocessor&)            = delete;
        preprocessor& operator=(const preprocessor&) = delete;

        // Non-movable
        preprocessor(preprocessor&&)            = delete;
        preprocessor& operator=(preprocessor&&) = delete;

        [[nodiscard]] status process(cstrview trimmed_line)
        {
            auto rng = trimmed_line.range();
            if (rng.drop_front('#'))
            {
                rng.pop_front_while(is_space_);
                const cstrview token{rng.pop_front_while(chr::is_alpha_lower)};
                rng.pop_front_while(is_space_);

                if (token == "if")
                {
                    stack_.append_inplace(state_, if_statement_handled_);

                    if_statement_handled_ = true;
                    if (state_ == compile)
                    {
                        state_ = parse_expression_(rng);
                        if (state_ == skip)
                        {
                            if_statement_handled_ = false;
                        }
                    }
                }
                else if (token == "elif")
                {
                    if (!if_statement_handled_)
                    {
                        state_ = parse_expression_(rng);
                        if (state_ != skip)
                        {
                            if_statement_handled_ = true;
                        }
                    }
                    else if (state_ == compile)
                    {
                        state_ = skip;
                    }
                }
                else if (token == "else")
                {
                    if (!if_statement_handled_)
                    {
                        state_                = compile;
                        if_statement_handled_ = true;
                    }
                    else if (state_ == compile)
                    {
                        state_ = skip;
                    }
                }
                else if (token == "endif")
                {
                    if (stack_)
                    {
                        const auto p = stack_.back(assume::not_empty);
                        stack_.drop_back(assume::not_empty);

                        state_                = p.first;
                        if_statement_handled_ = p.second;
                    }
                }
            }

            return state_;
        }

      private:
        const map::sorted<str, str>& predefined_macros_;
        const vec<str>& include_paths_;

        vec<pair::first_second<status, bool>> stack_;

        status state_              = compile;
        bool if_statement_handled_ = false;

        bool has_include_(const cstrview include)
        {
            str file_path;
            for (const auto& path : include_paths_)
            {
                file_path.clear();
                file_path << path << include;
                if (file::is_regular(file_path))
                {
                    return true;
                }
            }
            return false;
        }

        bool is_defined_(const cstrview macro) const
        {
            return predefined_macros_.contains(macro);
        }

        static bool is_space_(const char c) noexcept
        {
            return c == ' ' || c == '\t';
        }

        status parse_expression_(cstrrng rng)
        {
            bool negation = false;
            if (rng.drop_front('!'))
            {
                negation = true;
            }

            if (rng.drop_front("defined("))
            {
                const auto macro = rng.pop_front_while(fn::is{fn::not_equal_to{}, ')'}).view();
                if (validator::is_macro(macro))
                {
                    if (rng.drop_front(')') && rng.is_empty())
                    {
                        if (is_defined_(macro))
                        {
                            return negation ? skip : compile;
                        }
                        else
                        {
                            return negation ? compile : skip;
                        }
                    }
                }
            }
            else if (rng.drop_front("__has_include(<"))
            {
                const auto include = rng.pop_front_while(fn::is{fn::not_equal_to{}, '>'}).view();
                if (validator::is_file_path(include))
                {
                    if (rng.drop_front(">)") && rng.is_empty())
                    {
                        if (has_include_(include))
                        {
                            return negation ? skip : compile;
                        }
                        else
                        {
                            return negation ? compile : skip;
                        }
                    }
                }
            }

            return not_understood;
        }
    };
}
