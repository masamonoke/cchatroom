#pragma once

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif __GNUC__
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#endif

#define die(msg, ...) die_impl(__FUNCTION__, __LINE__, __FILE__, msg, ##__VA_ARGS__)

#ifdef __clang__
#pragma clang diagnostic pop
#endif

#define not_implemented() die("NOT_IMPLEMENTED")

void die_impl(const char* func_name, int line, const char* file, const char* msg, ...);
