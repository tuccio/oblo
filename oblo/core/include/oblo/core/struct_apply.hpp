#pragma once

#if __cpp_concepts >= 202002L && defined(__clang__) && __clang_major__ < 19 // TODO: Check for other compilers support somewhere down the line
    #include <oblo/core/detail/struct_apply_cpp20.hpp>
#else
    #include <oblo/core/detail/struct_apply_cpp17.hpp>
#endif