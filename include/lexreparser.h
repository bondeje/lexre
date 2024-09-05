#ifndef LEXREPARSER_H
#define LEXREPARSER_H

#include <string.h>

#include "peggy/parser.h"
#include "peggy/mempool.h"
#include "re.h"
#include "reutils.h"

#include "nfa.h"
#include "dfa.h"

typedef struct NFABuilder {
    Parser parser;
    NFA * nfa; // nfa is built while parsing
    HASH_MAP(pSymbol, pSymbol) symbol_map;
    MemPoolManager * sym_pool;
} NFABuilder;

typedef struct NFANode {
    ASTNode node;
    NFAState * start;   // the initial state of the subNFA
    NFAState * final;     // the final state of the subNFA
} NFANode;

BUILD_ALIGNMENT_STRUCT(NFANode)

void NFABuilder_init(NFABuilder * bldr, NFA * nfa);
void NFABuilder_dest(NFABuilder * bldr);
int NFABuilder_build_DFA(NFABuilder * bldr, char const * regex, size_t regex_len, DFA * dfa);

ASTNode * re_build_symbol(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_char_class(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_lookaround(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_element(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_repeated(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_sequence(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_choice(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_lexre(Production * prod, Parser * parser, ASTNode * node);


#endif

