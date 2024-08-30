#include <assert.h>
#include "lexreparser.h"

// High-level TODO: get rid of LexreBuilder_build...it doesn't work on types with #define

// node is the simple_escape node
void get_simple_escape_character(ASTNode * node, char * byte) {
    *byte = *node->children[1]->token_start->string;
    switch (*byte) {
        case 'a': {
            *byte = '\a';
            break;
        }
        case 'b': {
            *byte = '\b';
            break;
        }
        case 'f': {
            *byte = '\f';
            break;
        }
        case 'n': {
            *byte = '\n';
            break;
        }
        case 'r': {
            *byte = '\r';
            break;
        }
        case 't': {
            *byte = '\t';
            break;
        }
        case 'v': {
            *byte = '\v';
            break;
        }
        case '\\': {
            *byte = '\\';
            break;
        }
        case '\'': {
            *byte = '\'';
            break;
        }
        case '"': {
            *byte = '"';
            break;
        }
        case '?': {
            *byte = '?';
            break;
        }
    }
}

#define CHa 'a'
#define CHA 'A'
#define CH0 '0'
#define CH9 '9'
#define _10000 16

unsigned char get_hex_value(char byte) {
    if (byte >= CH0 && byte <= CH9) {
        return (unsigned char)byte;
    } else if (byte >= CHa) {
        return (unsigned char)(byte - (CHa - 10));
    }
    return (unsigned char)(byte - (CHA - 10));
}

// byte must be an array with two points
static inline unsigned char get_hex_byte(char byte[2]) {
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
    char * vals = node->children[1]->children[1]->token_start->string;
    bytes[0] = get_hex_byte(vals);
    bytes[1] = get_hex_byte(vals + 2);
    if (node->children[1]->children[0]->rule->id == UNICODE8) {
        bytes[2] = get_hex_bytes(vals + 4);
        bytes[3] = get_hex_bytes(vals + 6);
        *len = 4;       
    }
}

ASTNode * re_build_range(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
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
    ASTNode * new_node = Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));

    if (len_low == 1) {
        ((LexreNode *)new_node)->expr = (RegExpr *)LexreBuilder_build(SRangeExpr, reb, ch_low, ch_high);
    } else {
        ((LexreNode *)new_node)->expr = (RegExpr *)LexreBuilder_build(RangeExpr, reb, to_uint32_t(ch_low, len_low), to_uint32_t(ch_high, len_high), len_low);
    }
    return new_node;
}

ASTNode * re_build_char_class(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;

    unsigned char nchar = 0;
    unsigned int nunic = 0;
    unsigned int nrange = 0;
    ASTNode ** elements = node->children[2]->children;
    for (size_t i = 0; i < node->children[2]->nchildren; i++) {
        switch (elements[i]->rule->id) {
            case RANGE: {
                nrange++;
                break;
            }
            case ESCAPED_CHARACTER: {
                if (elements[i]->children[0]->rule->id == SIMPLE_ESCAPE) {
                    nchar++;
                } else {
                    nunic++;
                }
                break;
            }
            case CHARACTER_CLASS: {
                nchar++;
                break;
            }
        }
    }

    // because of the construction of char_class, nsub > 0 should be guaranteed
    unsigned int nsub = (nchar ? 1 : 0) + nunic + nrange;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    RegExpr ** subexprs = MemPoolManager_aligned_alloc(reb->mgr, sizeof(RegExpr *) * nsub, sizeof(RegExpr *));
    char * sym = LexreBuilder_buffer(reb, nchar);
    unsigned char sym_len = 0;
    // no need to allocate for ranges
    unsigned int j = 0; // index of subexpr
    for (size_t i = 0; i < node->children[2]->nchildren; i++) {
        switch (elements[i]->rule->id) {
            case RANGE: {
                subexprs[j++] = ((LexreNode *)elements[i])->expr;
                break;
            }
            case ESCAPED_CHARACTER: {
                if (elements[i]->children[0]->rule->id == SIMPLE_ESCAPE) {
                    sym[sym_len++] = elements[i]->children[0]->children[1]->token_start->string[0];
                } else {
                    unsigned char unic_len = 0;
                    get_unicode(elements[i]->children[0], NULL, &unic_len);
                    char * unic = LexreBuilder_buffer(reb, unic_len);
                    get_unicode(elements[i]->children[0], unic, &unic_len);
                    subexprs[j++] = LexreBuilder_build(Symbol, reb, unic, unic_len);
                }
                break;
            }
            case CHARACTER_CLASS: {
                sym[sym_len++] = elements[i]->token_start->string[0];
                break;
            }
        }
    }

    if (sym_len) {
        subexprs[j++] = LexreBuilder_build(CharSet, reb, sym, sym_len);
    }
    new_node->expr = (RegExpr *)LexreBuilder_build(CharClass, reb, subexprs, nsub, node->children[1]->nchildren);
    return (ASTNode *)new_node;
}

ASTNode * re_build_symbol(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    switch (node->rule->id) {
        case CARET: {
            new_node->expr = (RegExpr *)&bos_symbol;
            break;
        }
        case DOLLAR: {
            new_node->expr = (RegExpr *)&eos_symbol;
            break;
        }
        case NON_ESCAPE: {
            char * sym = LexreBuilder_buffer(reb, 1);
            sym[0] = node->token_start->string[0];
            new_node->expr = (RegExpr *)LexreBuilder_build(Symbol, reb, sym, 1);
            break;
        }
        default: { // escaped_character
            node = node->children[0];
            if (node->rule->id == SIMPLE_ESCAPE) {
                char * sym = LexreBuilder_buffer(reb, 1);
                get_simple_escape_character(node, sym);
                new_node->expr = (RegExpr *)LexreBuilder_build(Symbol, reb, sym, 1);
            } else { // unicode
                unsigned char unic_len = 0;
                get_unicode(node, NULL, &unic_len);
                char * unic = LexreBuilder_buffer(reb, unic_len);
                get_unicode(node, unic, &unic_len);
                new_node->expr = (RegExpr *)LexreBuilder_build(Symbol, reb, unic, unic_len);
            }
            break;
        }
    }
    return (ASTNode *)new_node;
}

ASTNode * re_build_lookahead(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    new_node->expr = LookaheadExpr_new(reb->mgr, (RegExpr *)((LexreNode *)node)->expr, node->children[2]->rule->id == EQUALS);
    return (ASTNode *)new_node;
}

ASTNode * re_build_lookbehind(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    
    unsigned char sym_len = 0;
    ASTNode ** elements = node->children[4]->children;
    for (size_t i = 0; i < node->children[4]->nchildren; i++) {
        if (elements[i]->rule->id = NON_RPAREN) {
            sym_len++;
        } else if (elements[i]->children[0]->rule->id == SIMPLE_ESCAPE) {
            sym_len++;
        } else { // unicode
            unsigned char unic_len = 0;
            get_unicode(elements[i]->children[0], NULL, &unic_len);
            sym_len += unic_len;
        }
    }

    char * sym = LexreBuilder_buffer(reb, sym_len);
    unsigned char j = 0;
    for (size_t i = 0; i < node->children[4]->nchildren; i++) {
        if (elements[i]->rule->id = NON_RPAREN) {
            sym[j++] = elements[i]->token_start->string[0];
        } else if (elements[i]->children[0]->rule->id == SIMPLE_ESCAPE) {
            get_simple_escape_character(elements[i]->children[0], sym + j++);
        } else { // unicode
            unsigned char unic_len = 0;
            get_unicode(elements[i]->children[0], sym + j, &unic_len);
            j += unic_len;
        }
    }
}

ASTNode * re_build_element(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 3 && node->children[0]->rule->id == LPAREN) { // of form '(', choice, ')'
        *node->children[1] = *node; // copy node metadata into choice result (which is really an NFANode)
        //node->children[1]->rule = (Rule *)prod;
        return node->children[1];
    }
    //node->rule = (Rule *)prod;
    return node; // symbol should already be reduced
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
        return node;
    }

    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    ASTNode * rep = node->children[1]->children[0];
    unsigned int min_rep = 0;
    unsigned int max_rep = 0;
    switch (rep->rule->id) {
        case PLUS: {
            min_rep = 0;
            break;
        }
        case ASTERISK: {
            // do nothing
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
                break;
            }
            get_digit_seq_decimal(rep->children[3], &max_rep);
            break;
        }
    }
    new_node->expr = RepExpr_new(reb->mgr, ((LexreNode *)node)->expr, min_rep, max_rep);
    return (ASTNode *)new_node;
}

ASTNode * re_build_sequence(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 1) {
        return node;
    }
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));

    RegExpr ** subexprs = MemPoolManager_aligned_alloc(reb->mgr, sizeof(RegExpr *) * node->nchildren, sizeof(RegExpr *));
    for (size_t i = 0; i < node->nchildren; i++) {
        subexprs[i] = ((LexreNode *)node->children[i])->expr;
    }

    new_node->expr = SeqExpr_new(reb->mgr, subexprs, node->nchildren);
    return (ASTNode *)new_node;
}

ASTNode * re_build_choice(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 1) {
        return node;
    }
    unsigned int nsub = (node->nchildren + 1) >> 1;
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));

    RegExpr ** subexprs = MemPoolManager_aligned_alloc(reb->mgr, sizeof(RegExpr *) * nsub, sizeof(RegExpr *));
    for (size_t i = 0; i < node->nchildren; i += 2) {
        subexprs[i] = ((LexreNode *)node->children[i])->expr;
    }

    new_node->expr = ChoiceExpr_new(reb->mgr, subexprs, nsub);
    return (ASTNode *)new_node;
}

ASTNode * re_build_lexre(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    reb->entry = ((LexreNode *)node)->expr;
    return node;
}

