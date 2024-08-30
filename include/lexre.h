#ifndef LEXRE_H
#define LEXRE_H

#include "peggy/mempool.h"
#include "reutils.h"
//#include "dfa.h"
#include "regexpr.h"

#ifndef REGEX_STATIC_BUFFER_SIZE
#define REGEX_STATIC_BUFFER_SIZE 128
#endif

#define Lexre_get_byte(plexre) (plexre)->string[(plexre)->cursor]
#define Lexre_seek2_SEEK_END(plexre, loc) ((plexre)->cursor = (plexre)->size - 1 - loc)
#define Lexre_seek2_SEEK_SET(plexre, loc) ((plexre)->cursor = loc)
#define Lexre_seek2_SEEK_CUR(plexre, loc) ((plexre)->cursor += loc)
#define Lexre_seek2(plexre, loc, dir) CAT(Lexre_seek2_, dir)(plexre, loc)
#define Lexre_seek1(plexre, loc) Lexre_seek2(plexre, loc, SEEK_CUR)
#define Lexre_seek(plexre, ...) CAT(Lexre_seek, VARIADIC_SIZE(__VA_ARGS__))(plexre, __VA_ARGS__)
#define Lexre_tell(plexre) (plexre)->cursor
#define Lexre_remain(plexre) ((plexre)->size - (plexre)->cursor)

#define Lexre_advance1(plexre) (plexre)->cursor++
#define Lexre_advance2(plexre, nbytes) Lexre_seek2_SEEK_CUR(plexre, nbytes)
#define Lexre_advance(...) CAT(Lexre_advance, VARIADIC_SIZE(__VA_ARGS__))(__VA_ARGS__)

typedef struct Lexre {
    struct RegExpr const * entry;
    MemPoolManager const * mgr; // memory pool for the RegExpr and subexpressions
    char const * string;
    size_t size;
    size_t cursor;
    int end;      // position of last accepting match
    unsigned int flags;     // flags for operation of the DFA. flags should account for the properties of the buffer (user provided)
} Lexre;


struct MatchString {
    char const * str;
    int len;
};

char * lexre_compile_pattern_buffered(const char * regex, const int regex_size,
    struct Lexre * pattern_buffer, unsigned int flags, char * buffer, 
    const int buffer_size);

char * lexre_compile_pattern(const char * regex, const int regex_size,
    struct Lexre * av, unsigned int flags);

int lexre_match(struct Lexre * av, const char * string, size_t size,
    size_t start);

// for streaming mode
int lexre_update(struct Lexre * av, const char * string, size_t size, size_t * cursor);

void lexre_get_match(struct Lexre * av, struct MatchString * match, 
    struct MatchString * unmatched);

static inline void lexre_reset(struct Lexre * av) {
    av->cursor = 0;
    av->end = -1;
}

void lexre_free(struct lexre * av);

int lexre_fprint(FILE * stream, struct lexre * av, HASH_MAP(pSymbol, pSymbol) * sym_map);

#endif
