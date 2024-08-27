#ifndef LEXRE_H
#define LEXRE_H

#include "reutils.h"
#include "dfa.h"

#ifndef REGEX_STATIC_BUFFER_SIZE
#define REGEX_STATIC_BUFFER_SIZE 128
#endif

struct lexre {
    DFA dfa;
    int cur_state;       // internal use state tracking
    char * buffer;          // buffer of characters passed to DFA
    int ibuffer;            // location of cursor in buffer
    int buffer_size;        // size of buffer
    int end;                // end of match in buffer(one past). -1 if no matches found
    unsigned int flags;     // flags for operation of the DFA. flags should account for the properties of the buffer (user provided)
};

struct MatchString {
    char const * str;
    int len;
};

char * lexre_compile_pattern_buffered(const char * regex, const int regex_size,
    struct lexre * pattern_buffer, unsigned int flags, char * buffer, 
    const int buffer_size);

char * lexre_compile_pattern(const char * regex, const int regex_size,
    struct lexre * av,
    unsigned int flags);

int lexre_match(struct lexre * av, const char * string, size_t size,
    size_t start);

// for streaming mode
int lexre_update(struct lexre * av, const char * string, size_t size, size_t * cursor);

void lexre_get_match(struct lexre * av, struct MatchString * match, 
    struct MatchString * unmatched);

static inline void lexre_reset(struct lexre * av) {
    av->cur_state = 0;
    av->ibuffer = 0;
    av->end = -1;
}

void lexre_free(struct lexre * av);

int lexre_fprint(FILE * stream, struct lexre * av, HASH_MAP(pSymbol, pSymbol) * sym_map);

#endif
