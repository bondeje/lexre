#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include "nfa.h"

#define DEFAULT_NFA_POOL_CT 128
#define DEFAULT_NFA_SYMBOL_CT 32

void NFA_init(NFA * nfa) {
    *nfa = (NFA){0};
    size_t size = sizeof(NFATransition) > sizeof(NFAState) ? sizeof(NFATransition) : sizeof(NFAState);
    nfa->states_cap = DEFAULT_NFA_POOL_CT;
    nfa->states = MemPoolManager_aligned_alloc(nfa->nfa_pool, nfa->states_cap * sizeof(NFAState *), sizeof(NFAState *));
}

void NFA_dest(NFA * nfa) {
    MemPoolManager_del(nfa->nfa_pool);
    if (nfa->sym_pool) {
        MemPoolManager_del(nfa->sym_pool);
    }
    *nfa = (NFA){0};
}

NFAState * NFA_new_state(NFA * nfa) {
    NFAState * out = MemPoolManager_aligned_alloc(nfa->nfa_pool, sizeof(NFAState), _Alignof(NFAState));
    if (nfa->nstates >= nfa->states_cap) {
        NFAState ** new_states = MemPoolManager_aligned_alloc(nfa->nfa_pool, 2 *nfa->states_cap * sizeof(NFAState *), sizeof(NFAState *));
        if (!new_states) {
            return NULL;
        }
        memcpy(new_states, nfa->states, sizeof(NFAState *) * nfa->states_cap);
        nfa->states_cap *= 2;
        nfa->states = new_states;
    }
    out->id = nfa->nstates;
    nfa->states[nfa->nstates++] = out;
    return out;
}

NFATransition * NFA_new_transition(NFA * nfa) {
    return MemPoolManager_aligned_alloc(nfa->nfa_pool, sizeof(NFATransition), _Alignof(NFATransition));
}
