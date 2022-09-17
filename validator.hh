// Copyright (c) 2022 Mikael Simonsson <https://mikaelsimonsson.com>.
// SPDX-License-Identifier: BSL-1.0

#pragma once

#include "snn-core/chr/common.hh"
#include "snn-core/fn/common.hh"
#include "snn-core/range/contiguous.hh"
#include "snn-core/string/range/split.hh"

namespace snn::app
{
    struct validator final
    {
        [[nodiscard]] static constexpr bool is_compiler(const transient<cstrview> s) noexcept
        {
            // Match: (clang|g)\+\+(-devel|[0-9]{0,2})
            auto rng = s.get().range();
            if (rng.drop_front("clang") || rng.drop_front('g'))
            {
                if (rng.drop_front("++-devel"))
                {
                    return rng.is_empty();
                }

                if (rng.drop_front("++"))
                {
                    return rng.count() <= 2 && rng.all(chr::is_digit);
                }
            }
            return false;
        }

        [[nodiscard]] static constexpr bool is_base(const transient<cstrview> s) noexcept
        {
            // Match: \.?[A-Za-z]([A-Za-z0-9._-]*[A-Za-z0-9])?
            auto rng = s.get().range();
            rng.drop_front('.'); // Allow hidden files.
            if (rng.has_front_if(chr::is_alpha) && rng.has_back_if(chr::is_alphanumeric))
            {
                rng.pop_front_while(
                    fn::is_any_of{chr::is_alphanumeric, fn::in_array{'.', '_', '-'}});
                return rng.is_empty();
            }
            return false;
        }

        [[nodiscard]] static constexpr bool is_directory(const transient<cstrview> s) noexcept
        {
            auto rng = s.get().range();

            // Skip prefixes if any.
            rng.drop_front('/');
            rng.drop_front("./");
            while (rng.drop_front("../"))
            {
            }

            while (rng)
            {
                if (!is_base(rng.pop_front_while(fn::is{fn::not_equal_to{}, '/'}).view()))
                {
                    return false;
                }

                if (!rng.drop_front('/'))
                {
                    return false;
                }
            }

            return true;
        }

        [[nodiscard]] static constexpr bool is_file_path(const transient<cstrview> s) noexcept
        {
            auto rng = s.get().range();
            if (is_base(rng.pop_back_while(fn::is{fn::not_equal_to{}, '/'}).view()))
            {
                return is_directory(rng.view());
            }
            return false;
        }

        [[nodiscard]] static constexpr bool is_library(const transient<cstrview> s) noexcept
        {
            if (s.get().size() <= 40) // Arbitrary
            {
                auto rng = s.get().range();
                if (rng.has_front_if(chr::is_alpha) && rng.has_back_if(chr::is_alphanumeric))
                {
                    rng.pop_front_while(
                        fn::is_any_of{chr::is_alphanumeric, fn::in_array{'_', '-', '.'}});
                    return rng.is_empty();
                }
            }
            return false;
        }

        [[nodiscard]] static constexpr bool is_macro(const transient<cstrview> s) noexcept
        {
            auto rng = s.get().range();
            if (rng.has_front_if(chr::is_alpha) || rng.has_front('_'))
            {
                rng.pop_front_while(
                    fn::is_any_of{chr::is_alphanumeric, fn::is{fn::equal_to{}, '_'}});
                return rng.is_empty();
            }
            return false;
        }

        [[nodiscard]] static constexpr bool is_reserved_target(
            const transient<cstrview> dir, const transient<cstrview> base) noexcept
        {
            // In GNU make "./targetname" seems to conflict with "targetname", so don't allow it.
            // E.g. "./all" conflicts with "all".

            const cstrview d = dir.get();
            if (d.is_empty() || d == "./")
            {
                const cstrview b = base.get();

                // General

                if (b == "all")
                {
                    return true;
                }

                if (b == "run")
                {
                    return true;
                }

                if (b == "clean")
                {
                    return true;
                }

                if (b == "clean-executables")
                {
                    return true;
                }

                if (b == "clean-object-files")
                {
                    return true;
                }

                if (b == "destruct")
                {
                    return true;
                }

                // Fuzzer

                if (b == "minimize-corpus")
                {
                    return true;
                }

                if (b == "compress-corpus")
                {
                    return true;
                }
            }

            return false;
        }
    };
}
