#include "lexreparser.h"
#include "thompson.h"

#define SYM_POOL_INIT_COUNT 16


void NFABuilder_init(NFABuilder * bldr, NFA * nfa) {
    // build and initialize an empy symbol transition
    Parser_init(&bldr->parser, (Rule *)&re_token, (Rule *)&re_re, RE_NRULES, 0);
    Parser_set_log_file((Parser *)bldr, "lexre.log", LOG_LEVEL_DEBUG);
    bldr->nfa = nfa;
    bldr->sym_pool = MemPoolManager_new(SYM_POOL_INIT_COUNT, sizeof(Symbol), _Alignof(Symbol));
    HASH_MAP_INIT(pSymbol, pSymbol)(&bldr->symbol_map, SYM_POOL_INIT_COUNT + 1);
}

void NFABuilder_segment_symbols(NFABuilder * bldr) {
    // TODO
}

// not in header
int symbol_map_to_array(void * sym_arr_, Symbol * key, Symbol * value) {
    struct SymbolArray * sym_arr = (struct SymbolArray *)sym_arr_;
    sym_arr->symbols[sym_arr->nsymbols++] = value;
    return 0;
}

int NFABuilder_build_DFA(NFABuilder * bldr, char const * regex, size_t regex_len, DFA * dfa) {
    NFA * nfa_old = bldr->nfa;
    NFA nfa;
    NFA_init(&nfa);
    bldr->nfa = &nfa;
    Parser_parse(&bldr->parser, regex, regex_len);
    int status = Parser_is_fail_node((Parser *)bldr, ((Parser *)bldr)->ast);
    if (status) {
        goto cleanup;
    }

    // TODO: split intersecting symbols
    NFABuilder_segment_symbols(bldr);
    
    // copy symbols from symbol map into NFA
    struct SymbolArray sym_arr = {
        .symbols = MemPoolManager_aligned_alloc(nfa.sym_pool, sizeof(Symbol *) * bldr->symbol_map.fill, sizeof(Symbol *)), 
        .nsymbols = 0
    };
    bldr->symbol_map._class->for_each(&bldr->symbol_map, symbol_map_to_array, &sym_arr);
    nfa.symbols = sym_arr.symbols;
    nfa.nsymbols = sym_arr.nsymbols;
    nfa.sym_pool = bldr->sym_pool;
    bldr->sym_pool = NULL;
    status = NFA_to_DFA(&nfa, dfa);

cleanup: // cleanup the NFA. NFABuilder cleanup responsibility of caller
    NFA_dest(&nfa);
    bldr->nfa = nfa_old;
    return status;
}

void NFABuilder_dest(NFABuilder * bldr) {
    Parser_dest(&bldr->parser);
    bldr->symbol_map._class->dest(&bldr->symbol_map);
    if (bldr->sym_pool) {
        MemPoolManager_del(bldr->sym_pool);
        bldr->sym_pool = NULL;
    }
    if (bldr->nfa) {
        NFA_dest(bldr->nfa);
    }
}

static inline Symbol * NFABuilder_get_symbol_ascii(NFABuilder * bldr, unsigned char sym) {
    Symbol * out = NULL;
    bldr->symbol_map._class->get(&bldr->symbol_map, (Symbol *)&(SymbolASCII){.sym = sym}, &out);
    return out; // even if get fails, out should equal NULL
}

static inline Symbol * NFABuilder_get_symbol_unicode(NFABuilder * bldr, uint32_t sym) {
    Symbol * out = NULL;
    bldr->symbol_map._class->get(&bldr->symbol_map, (Symbol *)&(SymbolUnicode){.sym = sym}, &out);
    return out; // even if get fails, out should equal NULL
}

static inline Symbol * NFABuilder_get_symbol_range_ascii(NFABuilder * bldr, unsigned char low, unsigned char high) {
    Symbol * out = NULL;
    bldr->symbol_map._class->get(&bldr->symbol_map, (Symbol *)&(SymbolRangeASCII){.low = low, .high = high}, &out);
    return out; // even if get fails, out should equal NULL
}

static inline Symbol * NFABuilder_get_symbol_lookaround(NFABuilder * bldr, char const * sym, unsigned char sym_len, unsigned char flags) {
    Symbol * out = NULL;
    bldr->symbol_map._class->get(&bldr->symbol_map, (Symbol *)&(SymbolLookaround){.sym = sym, .sym_len = sym_len, .flags = flags}, &out);
    return out; // even if get fails, out should equal NULL
}

static inline void NFABuilder_set_symbol(NFABuilder * bldr, Symbol * sym) {
    bldr->symbol_map._class->set(&bldr->symbol_map, sym, sym);
}

char get_simple_escape_character_(char byte) {
    switch (byte) {
        case 'a': return '\a';
        case 'b': return '\b';
        case 'f': return '\f';
        case 'n': return '\n';
        case 'r': return '\r';
        case 't': return '\t';
        case 'v': return '\v';
        case '\'': return '\'';
        case '"': return '"';
        case '?': return '?';
    }
    return byte;
}

// node is the simple_escape node
void get_simple_escape_character(ASTNode * node, char * byte) {
    *byte = get_simple_escape_character_(*node->children[1]->token_start->string);
}

#define CHa 'a'
#define CHA 'A'
#define CH0 '0'
#define CH9 '9'
#define _10 10
#define _10000 16

unsigned char get_hex_value(char const byte) {
    if (byte >= CH0 && byte <= CH9) {
        return (unsigned char)byte;
    } else if (byte >= CHa) {
        return (unsigned char)(byte - (CHa - _10));
    }
    return (unsigned char)(byte - (CHA - _10));
}

// byte must be an array with two points
static inline unsigned char get_hex_byte(char const byte[2]) {
    return get_hex_value(byte[0]) * _10000 + get_hex_value(byte[1]);
}

void get_unicode(ASTNode * node, char * bytes, unsigned char * len) {
    if (!bytes) {
        if (node->children[1]->children[0]->rule->id == UNICODE8) {
            *len = 4;
        } else {
            *len = 2;
        }
        return;
    }
    *len = 2;
    char const * vals = node->children[1]->children[1]->token_start->string;
    bytes[0] = get_hex_byte(vals);
    bytes[1] = get_hex_byte(vals + 2);
    if (node->children[1]->children[0]->rule->id == UNICODE8) {
        bytes[2] = get_hex_byte(vals + 4);
        bytes[3] = get_hex_byte(vals + 6);
        *len = 4;       
    }
}

ASTNode * re_build_range(Production * prod, Parser * parser, ASTNode * node) {
    NFABuilder * bldr = (NFABuilder *)parser;
    char ch_low[4] = {'\0'};
    char ch_high[4] = {'\0'};
    unsigned char len_low = 0;
    unsigned char len_high = 0;
    if (node->children[0]->rule->id == CHARACTER_CLASS) {
        len_low = 1;
        ch_low[0] = node->children[0]->token_start->string[0];
    } else if (node->children[0]->children[0]->rule->id == SIMPLE_ESCAPE) {
        len_low = 1;
        get_simple_escape_character(node->children[0]->children[0], ch_low);
    } else {
        get_unicode(node->children[0]->children[0], ch_high, &len_high);
    }

    if (node->children[2]->rule->id == CHARACTER_CLASS) {
        len_high = 1;
        ch_high[0] = node->children[2]->token_start->string[0];
    } else if (node->children[2]->children[0]->rule->id == SIMPLE_ESCAPE) {
        len_high = 1;
        get_simple_escape_character(node->children[2]->children[0], ch_high);
    } else {
        get_unicode(node->children[2]->children[0], ch_high, &len_high);
    }

    if (len_low != len_high) {
        return Parser_fail_node(parser);
    }

    // build symbol
    Symbol * new_sym = NULL;
    if (len_low == 1) {
        new_sym = NFABuilder_get_symbol_range_ascii(bldr, ch_low[0], ch_high[0]);
        if (!new_sym) {
            new_sym = (Symbol *)SymbolRangeASCII_new(ch_low[0], ch_high[0], bldr->sym_pool);
        }
    } else {
        // TODO: for unicode support
    }

    // build transition
    NFATransition * trans = NFA_new_transition(bldr->nfa);
    // build new states
    NFAState * final = NFA_new_state(bldr->nfa);
    NFAState * start = NFA_new_state(bldr->nfa);
    *start = (NFAState) {.n_out = 1, .n_in = 0, .out = trans, .id = start->id};
    *final = (NFAState) {.n_out = 0, .n_in = 1, .in = trans, .id = final->id};

    // initialize transition for start to final state
    *trans = (NFATransition) {.final = final, .next_in = NULL, .next_out = NULL, .start = start, .symbol = new_sym};

    // build new node
    NFANode * new_node = (NFANode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(NFANode));
    *new_node = (NFANode) {.node = *node, .start = start, .final = final};
    return (ASTNode *)new_node;
}

ASTNode * re_build_symbol(Production * prod, Parser * parser, ASTNode * node) {
    NFABuilder * bldr = (NFABuilder *)parser;
    Symbol * new_sym = NULL;
    switch (node->rule->id) {
        case CARET: {
            new_sym = sym_bos;
            NFABuilder_set_symbol(bldr, new_sym);
            break;
        }
        case DOLLAR: {
            new_sym = sym_eos;
            NFABuilder_set_symbol(bldr, new_sym);
            break;
        }
        case NON_ESCAPE: {
            unsigned char ch = ((unsigned char)0) + node->token_start->string[0];
            new_sym = NFABuilder_get_symbol_ascii(bldr, ch);
            if (!new_sym) {
                new_sym = (Symbol *)SymbolASCII_new(((unsigned char)0) + ch, bldr->sym_pool);
                NFABuilder_set_symbol(bldr, new_sym);
            }
            break;
        }
        case ESCAPED_CHARACTER: {
            if (node->children[0]->rule->id == SIMPLE_ESCAPE) {
                char ch_;
                get_simple_escape_character(node->children[0], &ch_);
                unsigned char ch = ((unsigned char)0) + ch;
                new_sym = NFABuilder_get_symbol_ascii(bldr, ch);
                if (!new_sym) {
                    new_sym = (Symbol *)SymbolASCII_new(((unsigned char)0) + ch, bldr->sym_pool);
                    NFABuilder_set_symbol(bldr, new_sym);
                }
            } else { // unicode_seq
                char ch[4] = {'\0'};
                unsigned char len = 0;
                get_unicode(node->children[0], ch, &len);
                uint32_t unic = to_uint32_t(ch, 4);
                new_sym = NFABuilder_get_symbol_unicode(bldr, unic);
                if (!new_sym) {
                    new_sym = (Symbol *)SymbolUnicode_new(to_uint32_t(ch, 4), bldr->sym_pool);
                    NFABuilder_set_symbol(bldr, new_sym);
                }
            }
            break;
        }
    }
    
    // build transition
    NFATransition * trans = NFA_new_transition(bldr->nfa);
    // build new states
    NFAState * final = NFA_new_state(bldr->nfa);
    NFAState * start = NFA_new_state(bldr->nfa);
    *start = (NFAState) {.n_out = 1, .n_in = 0, .out = trans, .id = start->id};
    *final = (NFAState) {.n_out = 0, .n_in = 1, .in = trans, .id = final->id};

    // initialize transition for start to final state
    *trans = (NFATransition) {.final = final, .next_in = NULL, .next_out = NULL, .start = start, .symbol = new_sym};

    // build new node
    NFANode * new_node = (NFANode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(NFANode));
    *new_node = (NFANode) {.node = *node, .start = start, .final = final};
    return (ASTNode *)new_node;
}

ASTNode * re_build_char_class(Production * prod, Parser * parser, ASTNode * node) {
    // TODO:
    return NULL;
}

void re_lookaround_preprocess(NFABuilder * bldr, ASTNode * node, _Bool behind, char const ** sym, unsigned char * len) {
    unsigned char max_len = (unsigned char)ASTNode_string_length(node);
    char const * src = node->children[0]->token_start->string;
    unsigned char i = 0, j = 0;
    char * dest = MemPoolManager_aligned_alloc(bldr->sym_pool, max_len, 1);
    dest[j++] = src[i++];   // '('
    dest[j++] = src[i++];   // '?'
    if (behind) {
        dest[j++] = src[i++];   // '<'
    }
    dest[j++] = src[i++];   // '='/'!'

    while (i < max_len) {
        if (src[i] == '\\') {
            i++;
            dest[j++] = get_simple_escape_character_(src[i++]);
        } else {
            dest[j++] = src[i++];
        }
    }
    *sym = dest;
    *len = j;
}

ASTNode * re_build_lookaround(Production * prod, Parser * parser, ASTNode * node) {
    NFABuilder * bldr = (NFABuilder *)parser;

    Symbol * new_sym = NULL;

    char const * sym = NULL;
    unsigned char sym_len = 0;
    unsigned char flags = 0;
    if (node->nchildren == 5) {
        re_lookaround_preprocess(bldr, node, false, &sym, &sym_len);
        flags |= SYMBOL_FLAG_FORWARD;
        if (node->children[2]->rule->id == EQUALS) {
            flags |= SYMBOL_FLAG_POSITIVE;
        }
    } else {
        re_lookaround_preprocess(bldr, node, true, &sym, &sym_len);
        if (node->children[3]->rule->id == EQUALS) {
            flags |= SYMBOL_FLAG_POSITIVE;
        }
    }
    
    new_sym = NFABuilder_get_symbol_lookaround(bldr, sym, sym_len, flags);
    if (!new_sym) {
        new_sym = (Symbol *)SymbolLookaround_new(sym, sym_len, flags, bldr->sym_pool);
    }

    // build transition
    NFATransition * trans = NFA_new_transition(bldr->nfa);
    // build new states
    NFAState * final = NFA_new_state(bldr->nfa);
    NFAState * start = NFA_new_state(bldr->nfa);
    *start = (NFAState) {.n_out = 1, .n_in = 0, .out = trans, .id = start->id};
    *final = (NFAState) {.n_out = 0, .n_in = 1, .in = trans, .id = final->id};

    // initialize transition for start to final state
    *trans = (NFATransition) {.final = final, .next_in = NULL, .next_out = NULL, .start = start, .symbol = new_sym};

    // build new node
    NFANode * new_node = (NFANode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(NFANode));
    *new_node = (NFANode) {.node = *node, .start = start, .final = final};
    return (ASTNode *)new_node;
}

NFATransition * re_build_empty_transition(NFABuilder * bldr, NFAState * start, NFAState * final) {
    NFATransition * trans = MemPoolManager_aligned_alloc(bldr->nfa->nfa_pool, sizeof(NFATransition), _Alignof(NFATransition));
    *trans = (NFATransition) {.final = final, .start = start, .next_in = final->in, .next_out = start->out, .symbol= sym_empty};
    final->in = trans;
    final->n_in++;
    start->out = trans;
    start->n_out++;
    return trans;
}

void get_digit_seq_decimal(ASTNode * node, unsigned int * dec) {
    static const char zero = '0';
    size_t n = node->nchildren;
    if (!n) {
        return;
    }
    char const * str = node->token_start->string;
    unsigned int val = 0;
    for (size_t i = 0; i < n; i++) {
        val = val * 10 + (str[i] - zero);
    }
    *dec = val;
}

ASTNode * re_build_repeated(Production * prod, Parser * parser, ASTNode * node) {
    if (!node->children[1]->nchildren) {
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    }

    NFABuilder * bldr = (NFABuilder *)parser;
    NFANode * element = (NFANode *)node->children[0];
    ASTNode * rep = node->children[1]->children[0];
    unsigned int min_rep = 0;
    unsigned int max_rep = 0;
    switch (rep->rule->id) {
        case PLUS: {
            min_rep = 1;
            break;
        }
        case ASTERISK: {
            break;
        }
        case QUESTION: {
            max_rep = 1;
            break;
        }
        default: {
            get_digit_seq_decimal(rep->children[1], &min_rep);
            if (!rep->children[2]->nchildren) {
                max_rep = min_rep;
            }
            get_digit_seq_decimal(rep->children[3], &max_rep);
        }
    }

    if (rep->rule->id == PLUS || (1 == min_rep && 0 == max_rep)) {
        // no new states, just add an empty transition back to start of element
        re_build_empty_transition(bldr, element->final, element->start);
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    } else if (rep->rule->id == QUESTION || (0 == min_rep && 1 == max_rep)) {
        // no new states, just add an empty transition back to start of element and from start to final
        re_build_empty_transition(bldr, element->start, element->final);
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    } else if (rep->rule->id == ASTERISK || (0 == min_rep && 0 == max_rep)) {
        // no new states, just add an empty transition back to start of element and from start to final
        re_build_empty_transition(bldr, element->final, element->start);
        re_build_empty_transition(bldr, element->start, element->final);
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    }

    // TODO: do a graph copy of element's subNFA for each min_rep > 1

    // TODO: append optional copies of element's subNFA for each max_rep > min_rep
    
    return Parser_fail_node(parser);
}

void re_merge_states(NFAState * start, NFAState * final) {
    // the start is the final node in a subNFA, which should have no transitions out
    // the final is the start node of a subsequent NFA, which should have all the transitions
    // for each transition in final, set the start state to start

    // handle final->out by setting the starts to start
    NFATransition * trans = final->out;
    NFATransition * trans_start = trans;
    trans->start = start;
    while (trans->next_out) {
        trans = trans->next_out;
        trans->start = start;        
    }
    trans->next_out = start->out;
    start->out = trans_start;
    start->n_out += final->n_out;

    // handle final->in by setting the finals to start
    if (final->in) {
        trans = final->in;
        trans_start = trans;
        trans->final = start;
        while (trans->next_in) {
            trans = trans->next_in;
            trans->final = start;
        }
        trans->next_in = start->in;
        start->in = trans_start;
        start->n_in += final->n_in;
    }
    
}

ASTNode * re_build_sequence(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 1) {
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    }

    NFABuilder * reb = (NFABuilder *)parser;
    NFAState * start = ((NFANode *)node->children[0])->start;
    NFAState * final = ((NFANode *)node->children[0])->final;
    for (size_t i = 1; i < node->nchildren; i++) {
        re_merge_states(final, ((NFANode *)node->children[i])->start);
        final = ((NFANode *)node->children[i])->final;
    }
    NFANode * new_node = (NFANode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(NFANode));
    new_node->node = *node;
    new_node->start = start;
    new_node->final = final;
    return (ASTNode *)new_node;
} 

ASTNode * re_build_choice(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 1) {
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    }

    NFABuilder * bldr = (NFABuilder *)parser;
    NFAState * start = ((NFANode *)node->children[0])->start;
    NFAState * final = ((NFANode *)node->children[0])->final;

    for (size_t i = 2; i < node->nchildren; i++) {
        re_merge_states(start, ((NFANode *)node->children[i])->start);
        re_merge_states(final, ((NFANode *)node->children[i])->final);
    }

    NFANode * new_node = (NFANode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(NFANode));
    new_node->node = *node;
    new_node->start = start;
    new_node->final = final;
    return (ASTNode *)new_node;
}

ASTNode * re_build_lexre(Production * prod, Parser * parser, ASTNode * node) {
    NFABuilder * bldr = (NFABuilder *)parser;
    NFANode * nfanode = (NFANode *)node;
    bldr->nfa->start = nfanode->start;
    bldr->nfa->final = nfanode->final;
    // TODO: this would be an appropriate place to finish initializing NFA instead of in NFABuilder_build_DFA
    return node;
}


ASTNode * re_build_element(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 3 && node->children[0]->rule->id == LPAREN) { // of form '(', choice, ')'
        *node->children[1] = *node; // copy node metadata into choice result (which is really an NFANode)
        node->children[1]->rule = (Rule *)prod;
        return node->children[1];
    }
    node->rule = (Rule *)prod;
    return node; // symbol should already be reduced
}