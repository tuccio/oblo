#pragma once

#define OBLO_STRINGIZE_IMPL(Name) #Name
#define OBLO_STRINGIZE(Name) OBLO_STRINGIZE_IMPL(Name)

#define OBLO_CAT(A, B) A##B
#define OBLO_CAT_EVAL(A, B) OBLO_CAT(A, B)