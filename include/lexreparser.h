#ifndef LEXREPARSER_H
#define LEXREPARSER_H

#include "peggy/parser.h"
#include "peggy/mempool.h"
#include "re.h"
#include "reutils.h"

#include "regexpr.h"

typedef struct LexreBuilder {
    Parser parser;
    RegExpr * entry;
    MemPoolManager * mgr; // memory pool for the RegExpr and subexpressions
} LexreBuilder;

#define LexreBuilder_build(type, pleb, ...) CAT(type, _new)((pleb)->mgr, __VA_ARGS__)
#define LexreBuilder_buffer(pleb, size) MemPoolManager_malloc((pleb)->mgr, size)
#define LexreBuilder_array(type, pleb, n) MemPoolManager_aligned_alloc((pleb)->mgr, sizeof(type) * (n), sizeof(type *))

typedef struct LexreNode {
    ASTNode node;
    RegExpr * expr;
} LexreNode;

ASTNode * re_build_range(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_char_class(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_symbol(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_lookahead(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_lookbehind(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_element(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_repeated(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_sequence(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_choice(Production * prod, Parser * parser, ASTNode * node);
ASTNode * re_build_lexre(Production * prod, Parser * parser, ASTNode * node);

#endif

