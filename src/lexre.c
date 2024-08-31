#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "lexre.h"
#include "lexreparser.h"

void Lexre_init(Lexre * av, LexreBuilder * reb, char const * regex, const int regex_size, unsigned int flags) {
    *av = (struct Lexre) {
        .entry = reb->entry,
        .mgr = reb->mgr,
        .regex_s = regex, 
        .regex_len = regex_size,
        .flags = flags,
        .end = -1
    };
    reb->entry = NULL;
    reb->mgr = NULL;
}

char * lexre_compile_pattern(const char * regex, const int regex_size,
    struct Lexre * av, unsigned int flags) {

    if (!regex_size || !regex) {
        return "no regex to compile";
    }
    
    LexreBuilder reb;
    LexreBuilder_init(&reb);
    LexreBuilder_parse(&reb, regex, regex_size);
    if (Parser_is_fail_node(((Parser *)&reb), ((Parser *)&reb)->ast)) {
        LexreBuilder_dest(&reb);
        return "parsing failed";
    }
    if (!reb.entry) {
        LexreBuilder_dest(&reb);
        return "no entry point found";
    }
    if (!reb.mgr) {
        LexreBuilder_dest(&reb);
        return "memory management failure";
    }

    Lexre_init(av, &reb, regex, regex_size, flags);    

    LexreBuilder_dest(&reb);
    
    return NULL;
}

int lexre_match(struct Lexre * av, const char * string, size_t const size,
    size_t const start) {
    
    if (start > size) {
        return -REGEX_FAIL;
    }
    av->cursor = 0;
    av->string = string + start;
    av->size = size - start;

    if (!av->entry->inter->eval(av, av->entry)) {
        return -REGEX_FAIL;
    }
    return (int)av->cursor;
}

void lexre_get_match(struct Lexre * av, struct MatchString * match) {
    match->str = av->string;
    match->len = (int)av->cursor;
}


void lexre_free(struct Lexre * av) {
    if (av->mgr) {
        MemPoolManager_del(av->mgr);
        av->mgr = NULL;
    }
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
/*
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

int lexre_fprint(FILE * stream, struct lexre * av, HASH_MAP(pSymbol, pSymbol) * sym_map) {
    int n = fprintf(stream, "{.flags = %u, .end = -1, .buffer_size = %d, .ibuffer = 0, .buffer = &(char[%d]){0}[0], .cur_state = 0, .dfa = ", av->flags, REGEX_STATIC_BUFFER_SIZE, REGEX_STATIC_BUFFER_SIZE);
    n += DFA_fprint(stream, (DFA *)av, sym_map);
    return n + fprintf(stream, "}");
}
*/
