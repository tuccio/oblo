#pragma once

#include <oblo/core/expected.hpp>
#include <oblo/core/string/cstring_view.hpp>
#include <oblo/core/string/string_builder.hpp>

#include "target_data.hpp"

namespace oblo::gen
{
    class reflection_worker
    {
    public:
        expected<> generate(const cstring_view sourceFile, const cstring_view outputFile, const target_data& target);

    private:
        void reset();

        void new_line();

        void indent(i32 i = 1);

        void deindent(i32 i = 1);

        void generate_record(const target_data& t, const record_type& r);
        void generate_enum(const enum_type& e);

    private:
        string_builder m_content;
        i32 m_indentation{};
    };
}