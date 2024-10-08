#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lexre.h"

#include "reparser.h"
#include "dfa.h"
#include "thompson.h"

char * lexre_compile_pattern_buffered(const char * regex, const int regex_size,
    struct lexre * av, unsigned int flags, char * buffer, 
    const int buffer_size) {

    *av = (struct lexre) {
        .dfa = {
            .regex_s = regex, 
            .regex_len = regex_size
        },
        .flags = flags, 
        .buffer = buffer, 
        .buffer_size = buffer_size,
        .end = -1
    };
    char * out = NULL;    
    NFA nfa;
    NFA_init(&nfa);
    RegexBuilder reb;
    RegexBuilder_init(&reb, &nfa);
    RegexBuilder_build_NFA(&reb, regex, regex_size);
    if (NFA_to_DFA(&nfa, (DFA *)av)) {
        out = "NFA to DFA conversion failure";
    }
    RegexBuilder_dest(&reb);
    NFA_dest(&nfa);
    return out;
}

char * lexre_compile_pattern(const char * regex, const int regex_size,
    struct lexre * av, unsigned int flags) {
    
    flags |= REGEX_ALLOC_BUFFER;
    char * buffer = malloc(sizeof(char) * REGEX_ALLOC_BUFFER_SIZE);
    if (!buffer) {
        return "buffer malloc failure in re_compile_pattern";
    }
    return lexre_compile_pattern_buffered(regex, regex_size, av, flags, buffer, REGEX_ALLOC_BUFFER_SIZE);
}

int lexre_match(struct lexre * av, const char * string, size_t const size,
    size_t const start) {
    
    if (start > size) {
        return -REGEX_FAIL;
    }
    size_t cur = start;
    int status = REGEX_WAIT;
    // note that cur==size must be allowed for EOS checks. successes at end of string must advance cursor
    while (cur <= size && (status = lexre_update(av, string, size, &cur)) >= REGEX_WAIT) {
        // do nothing, lexre_update will advance cursor
    }
    if (status == REGEX_WAIT) { // match never found but did not fail
        return -REGEX_FAIL;
    } else if (status == REGEX_WAIT_MATCH || status <= 0) { // match is successful but not necessarily on last input
        struct MatchString match;
        lexre_get_match(av, &match, NULL);
        return match.len;
    }
    // error or fail found
    return -status;
}

// size_t size here is different than the others. here, it is the length of the string up to the final character
static inline int av_push_buffer(struct lexre * av, const char * string, size_t const size, size_t const start) {
    size_t len = size - start;
    if (!len) {
        return 0;
    }
    if (av->buffer_size - (int)len < av->ibuffer) {
        return 1;
    }
    memcpy(av->buffer + av->ibuffer, string + start, len * sizeof(char));
    av->ibuffer += len;
    return 0;
}

/**
 * returns: 
 *      REGEX_MATCH if matches on last input
 *      REGEX_FAIL if no match at all
 *      REGEX_ERROR if internal error occurs
 *      REGEX_WAIT if no match but not fail
 *      integer x < 0 if match occurred -x characters ago
 * 
 * to use in real-time, *cursor == 0 should be true for each call
 */
int lexre_update(struct lexre * av, const char * string, size_t const size, size_t * cursor) {
    size_t const start = *cursor;
    if (DFA_check(DFA_get_state((DFA *)av, av->cur_state), string, size, cursor, &av->cur_state)) {
        if (av->end >= 0) {
            return (av->end - av->ibuffer);
        }
        return REGEX_FAIL;
    }
    // cursor can technically end up past the size of the string. clip it
    // this check should really be done in the DFA-check. test first. TODO
    if (av_push_buffer(av, string, *cursor > size ? size : *cursor, start)) {
        return REGEX_ERROR;
    }

    if (av->cur_state >= ((DFA *)av)->nstates) {
        return REGEX_ERROR;
    }
    if (DFA_is_accepting((DFA *)av, av->cur_state)) {
        av->end = av->ibuffer;
        if (!DFA_get_state((DFA *)av, av->cur_state)->trans || (av->flags & REGEX_NO_GREEDY)) { // NOTE: not sure this is really correct for "REGEX_NO_GREEDY"
            return REGEX_MATCH;
        } else {
            return REGEX_WAIT_MATCH;
        }
    } else if (0 <= av->end) {
        return REGEX_WAIT_MATCH;
    }
    return REGEX_WAIT;
}

void lexre_get_match(struct lexre * av, struct MatchString * match, 
    struct MatchString * unmatched) {
    
    match->str = av->buffer;
    match->len = av->end;
    if (unmatched) {
        unmatched->str = av->buffer + av->end;
        unmatched->len = av->ibuffer - av->end;
    }
}

void lexre_free(struct lexre * av) {
    if ((av->flags & REGEX_ALLOC_BUFFER)) {
        free(av->buffer);
    }
    DFA_dest(&av->dfa);
}

int lexre_fprint(FILE * stream, struct lexre * av, HASH_MAP(pSymbol, pSymbol) * sym_map) {
    int n = fprintf(stream, "{.flags = %u, .end = -1, .buffer_size = %d, .ibuffer = 0, .buffer = &(char[%d]){0}[0], .cur_state = 0, .dfa = ", av->flags, REGEX_STATIC_BUFFER_SIZE, REGEX_STATIC_BUFFER_SIZE);
    n += DFA_fprint(stream, (DFA *)av, sym_map);
    return n + fprintf(stream, "}");
}
