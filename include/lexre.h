#ifndef LEXRE_H
#define LEXRE_H

#include "reutils.h"
#include "dfa.h"

#ifndef REGEX_STATIC_BUFFER_SIZE
#define REGEX_STATIC_BUFFER_SIZE 128
#endif

#define Lexre_get_byte(plexre) (plexre)->string[(plexre)->cursor]
// final 2 is SEEK_END
#define Lexre_seek2_2(plexre, loc) ((plexre)->cursor = (plexre)->size - 1 - loc)
// final 0 is SEEK_SET
#define Lexre_seek2_0(plexre, loc) ((plexre)->cursor = loc)
// final 1 is SEEK_CUR
#define Lexre_seek2_SEEK_CUR(plexre, loc) ((plexre)->cursor += loc)
#define Lexre_seek2(plexre, loc, dir) CAT(Lexre_seek2_, dir)(plexre, loc)
#define Lexre_seek1(plexre, loc) Lexre_seek2(plexre, loc, SEEK_CUR)
#define Lexre_seek(plexre, ...) CAT(Lexre_seek, VARIADIC_SIZE(__VA_ARGS__))(plexre, __VA_ARGS__)
#define Lexre_tell(plexre) (plexre)->cursor
#define Lexre_remain(plexre) ((plexre)->size - (plexre)->cursor)

#define Lexre_advance1(plexre) (plexre)->cursor++
#define Lexre_advance2(plexre, nbytes) Lexre_seek2_SEEK_CUR(plexre, nbytes)
#define Lexre_advance(...) CAT(Lexre_advance, VARIADIC_SIZE(__VA_ARGS__))(__VA_ARGS__)

struct Lexre {
    DFA dfa;
    int cur_state;       // internal use state tracking
    unsigned char * string; 
    size_t size;
    unsigned int flags;     // flags for operation of the DFA. flags should account for the properties of the buffer (user provided)
    int end;
    int cursor;
};

typedef struct MatchString {
    unsigned char const * str;
    int len;
} MatchString;

char * lexre_compile_pattern(const char * regex, const int regex_size,
    Lexre * av,
    unsigned int flags);

int lexre_match(Lexre * av, const char * string, size_t size,
    size_t start);

// for streaming mode
int lexre_update(Lexre * av, const char * string, size_t size, size_t * cursor);

MatchString lexre_get_match(Lexre * av);

static inline void lexre_reset(Lexre * av) {
    av->cur_state = 0;
    av->cursor = 0;
    av->end = -1;
}

void lexre_free(Lexre * av);

int lexre_fprint(FILE * stream, Lexre * av, HASH_MAP(pSymbol, LString) * sym_map);

#endif
