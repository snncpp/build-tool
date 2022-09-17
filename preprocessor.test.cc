// Copyright (c) 2022 Mikael Simonsson <https://mikaelsimonsson.com>.
// SPDX-License-Identifier: BSL-1.0

#include "build-tool/preprocessor.hh"

#include "snn-core/unittest.hh"
#include "snn-core/ascii/pad.hh"
#include "snn-core/ascii/trim.hh"
#include "snn-core/string/range/split.hh"

namespace snn
{
    void unittest()
    {
        map::sorted<str, str> predefined_macros;
        predefined_macros.insert("__FreeBSD__", "1");

        vec<str> include_paths;
        include_paths.append("/usr/include/");

        app::preprocessor preprocessor{predefined_macros, include_paths};

        strbuf contents;

        contents << "#if defined(__FreeBSD__)\n";
        contents << "#if __has_include(<stdio.h>)\n";
        contents << "#include \"snn/example/impl/fbsd_stdio.hh\"\n";
        contents << "#else\n";
        contents << "#include \"snn/example/impl/fbsd.hh\"\n";
        contents << "#endif\n";
        contents << "#elif defined(__linux__)\n";
        contents << "#include \"snn/example/impl/linux.hh\"\n";
        contents << "#else\n";
        contents << "#include \"snn/example/impl/portable.hh\"\n";
        contents << "#endif\n";

        strbuf processed;
        strbuf tmpline;

        for (cstrview line : string::range::split{contents, '\n'})
        {
            ascii::trim_inplace(line);

            const auto status = preprocessor.process(line);

            tmpline = line;
            ascii::pad_right_inplace(tmpline, 50, ' ');

            switch (status)
            {
                case preprocessor.compile:
                    tmpline.append(" [compile]");
                    break;

                case preprocessor.skip:
                    tmpline.append(" [skip]");
                    break;

                case preprocessor.not_understood:
                    tmpline.append(" [not_understood]");
                    break;
            }

            tmpline.append('\n');
            processed.append(tmpline);

            if (line.has_front("#include \""))
            {
                continue;
            }
            else if (line.has_front("#include <"))
            {
                continue;
            }
            else if (line.is_empty() || line.has_front('#') || line.has_front("//"))
            {
                continue;
            }

            break;
        }

        constexpr cstrview expected =
            "#if defined(__FreeBSD__)                           [compile]\n"
            "#if __has_include(<stdio.h>)                       [compile]\n"
            "#include \"snn/example/impl/fbsd_stdio.hh\"          [compile]\n"
            "#else                                              [skip]\n"
            "#include \"snn/example/impl/fbsd.hh\"                [skip]\n"
            "#endif                                             [compile]\n"
            "#elif defined(__linux__)                           [skip]\n"
            "#include \"snn/example/impl/linux.hh\"               [skip]\n"
            "#else                                              [skip]\n"
            "#include \"snn/example/impl/portable.hh\"            [skip]\n"
            "#endif                                             [compile]\n"
            "                                                   [compile]\n";

        snn_require(processed == expected);
    }
}
