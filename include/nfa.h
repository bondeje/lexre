#ifndef NFA_H
#define NFA_H

#include <stddef.h>
#include "reutils.h"
#include "peggy/mempool.h"
#include "fa_.h"

typedef struct NFATransition NFATransition;

typedef unsigned int NFA_state;

typedef struct NFA NFA;

typedef struct NFAState NFAState;

struct NFATransition {
    NFAState * start; // not sure this one is necessary
    NFAState * final; // the final state of the transition
    Symbol * symbol;
    NFATransition * next_in; // next in linked list of transitions in a given state
    NFATransition * next_out; // next in linked list of transition out of a 
};

BUILD_ALIGNMENT_STRUCT(NFATransition)

struct NFAState {
    NFATransition * out; // linked list of transitions out of the state
    NFATransition * in;
    size_t n_out;
    size_t n_in;
    size_t id;
};

BUILD_ALIGNMENT_STRUCT(NFAState)

struct NFA {
    MemPoolManager * nfa_pool; // pool of data belonging to this nfa. The Transitions, the states, the preprocessed regex string
    // growing array of symbols. This will ultimately be transferred to the DFA definition
    // growing array of NFAStates
    MemPoolManager * sym_pool;
    Symbol ** symbols;
    size_t nsymbols;
    NFAState ** states;
    size_t nstates;
    size_t states_cap;
    // original regex string
    char const * regex_s;
    size_t regex_len;
    NFAState * start; // the initial state
    NFAState * final; // the final state. Used by DFA for the "accepting state"
    unsigned int flags; // for both NFA and DFA
};

BUILD_ALIGNMENT_STRUCT(NFA)

void NFA_init(NFA * nfa);

void NFA_dest(NFA * nfa);

NFAState * NFA_new_state(NFA * nfa);
Symbol * NFA_get_symbol_ascii(NFA * nfa, unsigned char sym);
Symbol * NFA_get_symbol_unicode(NFA * nfa, uint32_t sym);
Symbol * NFA_get_symbol_range_ascii(NFA * nfa, unsigned char low, unsigned char high);
Symbol * NFA_get_symbol_lookaround(NFA * nfa, char * sym, unsigned char sym_len, unsigned char flags);
NFATransition * NFA_new_transition(NFA * nfa);

#endif

