#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lexre.h"

#include "lexreparser.h"
#include "dfa.h"
#include "thompson.h"

char * lexre_compile_pattern(const char * regex, const int regex_size,
    Lexre * av, unsigned int flags) {
    
    *av = (Lexre) {
        .dfa = {
            .regex_s = regex, 
            .regex_len = regex_size,
        },
        .flags = flags,
    };

    char * out = NULL;    
    NFABuilder bldr;
    NFABuilder_init(&bldr, NULL);
    NFABuilder_build_DFA(&bldr, regex, regex_size, &av->dfa);
    NFABuilder_dest(&bldr);
    lexre_reset(av); // shouldn't be necessary

    return out;
}

int lexre_match(Lexre * av, const char * string, size_t const size,
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
        MatchString match = lexre_get_match(av);
        return match.len;
    }
    // error or fail found
    return -status;
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
int lexre_update(Lexre * av, const char * string, size_t const size, size_t * cursor) {
    size_t const start = *cursor;
    if (DFA_check(av, DFA_get_state((DFA *)av, av->cur_state), string, size, cursor, &av->cur_state)) {
        if (av->end >= 0) {
            return av->end;
        }
        return REGEX_FAIL;
    }

    if (av->cur_state >= ((DFA *)av)->nstates) {
        return REGEX_ERROR;
    }
    if (DFA_is_accepting((DFA *)av, av->cur_state)) {
        av->end = Lexre_tell(av);
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

MatchString lexre_get_match(Lexre * av) {
    if (av->end >= 0) {
        return (MatchString){.str = NULL, .len = 0};    
    }
    return (MatchString){.str = av->string, .len = av->end};
}

void lexre_free(Lexre * av) {
    DFA_dest(&av->dfa);
}

int lexre_fprint(FILE * stream, Lexre * av, HASH_MAP(pSymbol, LString) * sym_map) {
    int n = fprintf(stream, "{.flags = %u, .end = -1, .buffer_size = %d, .ibuffer = 0, .buffer = &(char[%d]){0}[0], .cur_state = 0, .dfa = ", av->flags, REGEX_STATIC_BUFFER_SIZE, REGEX_STATIC_BUFFER_SIZE);
    n += DFA_fprint(stream, (DFA *)av, sym_map);
    return n + fprintf(stream, "}");
}
