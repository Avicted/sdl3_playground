#pragma once // include guard

// Macros
#define Assert(Expression)                                                     \
    if (!(Expression))                                                         \
    {                                                                          \
        __builtin_trap();                                                      \
    }

// Defines
#define internal static
#define local_persist static
#define global_variable static

#define PI acos(-1.0)
