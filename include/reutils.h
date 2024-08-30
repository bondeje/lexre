#ifndef CAT
    #define CAT_(x, y) x##y
    #define CAT(x, y) CAT_(x, y)
#endif

#ifndef VARIADIC_SIZE
    #define VARIADIC_SIZE(...) VARIADIC_SIZE_(__VA_ARGS__, 64, 63, 62, 61, 60, 59, 58, 57, 56, 55, 54, 53, 52, 51, 50, 49, 48, 47, 46, 45, 44, 43, 42, 41, 40, 39, 38, 37, 36, 35, 34, 33, 32, 31, 30, 29, 28, 27, 26, 25, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0, UNUSED)
    #define VARIADIC_SIZE_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, _25, _26, _27, _28, _29, _30, _31, _32, _33, _34, _35, _36, _37, _38, _39, _40, _41, _42, _43, _44, _45, _46, _47, _48, _49, _50, _51, _52, _53, _54, _55, _56, _57, _58, _59, _60, _61, _62, _63, _64, size, ...) size
#endif

#if __STDC_VERSION__ < 201112L
#ifndef _Alignof
    #define _Alignof(type) offsetof(struct CAT(type, _ALIGNMENT), type)
#endif
    #define BUILD_ALIGNMENT_STRUCT(type) \
    struct CAT(type, _ALIGNMENT) {\
        char a;\
        type type;\
    };
#else
    #define BUILD_ALIGNMENT_STRUCT(type)
#endif

#define REGEX_ALLOC_BUFFER 1
#define REGEX_ALLOC_BUFFER_SIZE 128
#define REGEX_NO_GREEDY 2
#define REGEX_DOT_NO_NL 4

#define REGEX_MATCH 0
#define REGEX_FAIL 1
#define REGEX_ERROR 2
#define REGEX_WAIT 3
#define REGEX_WAIT_MATCH 4

uint32_t to_uint32_t(char * str, unsigned char str_len);
