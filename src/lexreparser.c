#include <assert.h>
#include "lexreparser.h"

void LexreBuilder_init(LexreBuilder * reb) {
    // build and initialize an empy symbol transition
    Parser_init(&reb->parser, (Rule *)&re_token, (Rule *)&re_re, RE_NRULES, 0);
    Parser_set_log_file((Parser *)reb, "lexre.log", LOG_LEVEL_DEBUG);
    reb->mgr = MemPoolManager_new(4000, 1, 1); // does not have to be freed unless parsing fails
    reb->entry = NULL;

}

void LexreBuilder_dest(LexreBuilder * reb) {
    if (reb->mgr) {
        MemPoolManager_del(reb->mgr);
        reb->mgr = NULL;
    }
    reb->entry = NULL;
    Parser_dest((Parser *)reb);
}

char * pp_realloc_buf(char * buf, size_t * buf_len, size_t new_size) {
    if (new_size <= *buf_len) {
        return buf;
    }
    char * new_buf = realloc(buf, new_size * sizeof(char));
    if (!new_buf) {
        return buf;
    }
    *buf_len = new_size;
    return new_buf;
}

char const * get_last_element(char const * start, char const * end) {
    end--;
    if (*end == ')' || *end == ']') {
        char open_ch = *end;
        char close_ch = open_ch - 1 - (open_ch > ')' ? 1 : 0);
        int open = 1;
        while (open && end != start) {
            end--;
            if (*end == open_ch) {
                open++;
            } else if (*end == close_ch) {
                open--;
            }
        }
        return end;
    }
    return end;
}

char * pp_repeatopt(char * buf, size_t * buf_len, char const * base, size_t base_length, size_t nrepeats, size_t * loc) {
    size_t space_needed = nrepeats * (base_length + 3) + *loc + 1; // add one for null-terminator
    if (*buf_len < space_needed) {
        space_needed = space_needed < 2 *(*buf_len) ? 2 * (*buf_len) : space_needed;
        size_t base_offset = (size_t) (base - buf);
        buf = pp_realloc_buf(buf, buf_len, space_needed);
        base = buf + base_offset;
    }
    for (size_t i = 0; i < nrepeats; i++) {
        int n = snprintf(buf + *loc, *buf_len - *loc, "(%.*s", (int)base_length, base);
        *loc += n;
    }
    for (size_t i = 0; i < nrepeats; i++) {
        int n = snprintf(buf + *loc, *buf_len - *loc, ")?");
        *loc += n;
    }
    return buf;
}

char * pp_repeat(char * buf, size_t * buf_len, char const * base, size_t base_length, size_t nrepeats, size_t * loc) {
    size_t space_needed = nrepeats * base_length + *loc + 1; // add one for null-terminator
    if (*buf_len < space_needed) {
        space_needed = space_needed < 2 *(*buf_len) ? 2 * (*buf_len) : space_needed;
        size_t base_offset = (size_t) (base - buf);
        buf = pp_realloc_buf(buf, buf_len, space_needed);
        base = buf + base_offset;
    }
    for (size_t i = 0; i < nrepeats; i++) {
        int n = snprintf(buf + *loc, *buf_len - *loc, "%.*s", (int)base_length, base);
        *loc += n;
    }
    return buf;
}

char * LexreBuilder_preprocess(LexreBuilder * reb, char const * regex_in, size_t regex_in_len, size_t * regex_out_len) {
    size_t buf_len = regex_in_len * 2; // allocate 2x times the size
    char * buf = malloc(sizeof(char) * buf_len);

    size_t i = 0, j = 0;
    while (i < regex_in_len) {
        size_t write = i;
        while (i < regex_in_len && *(regex_in + i) != '{' && *(regex_in + i) != '+') {
            i++;
        }
        // check for escaping
        size_t nescapes = 0;
        if (i) {
            size_t k = i - 1;
            while (regex_in[k--] == '\\') {
                nescapes++;
            }
        }
        if (nescapes & 1) { // if an odd amount of escapes, then '{' or '+' is escaped
            i++; // skip past. wait for write to continue
        }
        if (i > write) {
            buf = pp_realloc_buf(buf, &buf_len, j + (i - write));
            j += snprintf(buf + j, buf_len - j, "%.*s", (int)(i - write), regex_in + write);
        }
        if (nescapes & 1) { // if an odd amount of escapes, then '{' or '+' is escaped
            continue; // escaped the '{', '+'. continue
        }
        if (i >= regex_in_len) {
            break;
        }
        if (*(regex_in + i) == '{') {
            i++; // skip past '{'
            size_t min_rep = 0, max_rep = 0;
            char * next;
            if (*(regex_in + i) != ',') {
                min_rep = (size_t)strtoull(regex_in + i, &next, 10);
                i = (size_t)(next - regex_in);
                if (*next == '}') {// {min_rep}
                    max_rep = min_rep;
                } else { // must have ',' next
                    i++; // skip comma
                    while (*(regex_in + i) == ' ') {
                        i++;
                    }
                    if (*(regex_in + i) != '}') { // {min_rep, max_rep}
                        max_rep = (size_t)strtoull(regex_in + i, &next, 10);
                        i = (size_t)(next - regex_in);
                    }
                }
                i++; // skip closing brace
            } else { // min_rep = 0
                i++; // skip comma
                while (*(regex_in + i) == ' ') {
                    i++;
                }
                if (*(regex_in + i) != '}') { // {, max_rep}
                    max_rep = (size_t)strtoull(regex_in + i, &next, 10);
                    i = (size_t)(next - regex_in);
                }
                i++;
            }
            char const * start = get_last_element(buf, buf + j);
            if (max_rep == 0) {
                // replace A{min_rep,} with A1 A2 ... Am A*
                buf = pp_repeat(buf, &buf_len, start, (size_t)(buf + j - start), min_rep, &j);
                size_t start_offset = (size_t)(start - buf);
                buf = pp_realloc_buf(buf, &buf_len, j + 1);
                start = buf + start_offset;
                j += snprintf(buf + j, buf_len - j, "*"); // one rep was already printed, need one more (covered above with min_rep nrepeats) and add asterisk
            } else if (max_rep == 1) {
                if (min_rep == 1) { // replace with A
                    // do nothing, this is already done
                } else if (min_rep == 0) { // replace with A?
                    buf = pp_realloc_buf(buf, &buf_len, j + 1);
                    j += snprintf(buf + j, buf_len - j, "?");
                } else {
                    // ERROR!
                }
            } else { // replace A{m,n} with A1 A2...Am(Am+1(Am+2(...An-1(An)?)?)?)?
                size_t base_len = (size_t)(buf + j - start);
                size_t start_offset = (size_t)(start - buf);
                buf = pp_repeat(buf, &buf_len, start, base_len, min_rep - 1, &j); // min_rep - 1 because one should already have been printed
                start = buf + start_offset;
                buf = pp_repeatopt(buf, &buf_len, start, base_len, max_rep - min_rep, &j);
            }
        } else if (*(regex_in + i) == '+') {
            i++; // skip past '+'
            
            char const * start = get_last_element(buf, buf + j);
            // replace A+ with AA*
            size_t start_offset = (size_t)(start - buf);
            buf = pp_realloc_buf(buf, &buf_len, j + (size_t)(buf + j - start) + 1);
            start = buf + start_offset;
            j += snprintf(buf + j, buf_len - j, "%.*s*", (int)(regex_in - start + i), start);
        }
    }

    *regex_out_len = j;
    return buf;
}

void LexreBuilder_parse(LexreBuilder * reb, char const * regex_in, size_t regex_in_len) {
    /*
    size_t regex_out_len = 0;
    char * regex_out = LexreBuilder_preprocess(reb, regex_in, regex_in_len, &regex_out_len);
    if (!regex_out) {
        reb->parser.ast = Parser_fail_node((Parser *)reb);
        return;
    }
    printf("preprocessed: %.*s -> %.*s\n", (int)regex_in_len, regex_in, (int)regex_out_len, regex_out);
    Parser_parse((Parser *)reb, regex_out, regex_out_len);
    if (regex_out) {
        free(regex_out);
    }
    */
    printf("processing: %.*s\n", (int)regex_in_len, regex_in);
    Parser_parse((Parser *)reb, regex_in, regex_in_len);
}

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

unsigned char get_hex_value(char const byte) {
    if (byte >= CH0 && byte <= CH9) {
        return (unsigned char)byte;
    } else if (byte >= CHa) {
        return (unsigned char)(byte - (CHa - 10));
    }
    return (unsigned char)(byte - (CHA - 10));
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
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));

    if (len_low == 1) {
        new_node->expr = (RegExpr *)SRangeExpr_new(reb->mgr, ch_low[0], ch_high[0]);
    } else {
        new_node->expr = (RegExpr *)RangeExpr_new(reb->mgr, to_uint32_t(ch_low, len_low), to_uint32_t(ch_high, len_high), len_low);
    }
    return (ASTNode *)new_node;
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
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
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
                    subexprs[j++] = (RegExpr *)Symbol_new(reb->mgr, unic, unic_len);
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
        subexprs[j++] = (RegExpr *)CharSet_new(reb->mgr, sym, sym_len);
    }
    new_node->expr = (RegExpr *)CharClass_new(reb->mgr, subexprs, nsub, node->children[1]->nchildren > 0);
    return (ASTNode *)new_node;
}

ASTNode * re_build_symbol(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
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
            new_node->expr = (RegExpr *)Symbol_new(reb->mgr, sym, 1);
            break;
        }
        default: { // escaped_character
            node = node->children[0];
            if (node->rule->id == SIMPLE_ESCAPE) {
                char * sym = LexreBuilder_buffer(reb, 1);
                get_simple_escape_character(node, sym);
                new_node->expr = (RegExpr *)Symbol_new(reb->mgr, sym, 1);
            } else { // unicode
                unsigned char unic_len = 0;
                get_unicode(node, NULL, &unic_len);
                char * unic = LexreBuilder_buffer(reb, unic_len);
                get_unicode(node, unic, &unic_len);
                new_node->expr = (RegExpr *)Symbol_new(reb->mgr, unic, unic_len);
            }
            break;
        }
    }
    return (ASTNode *)new_node;
}

ASTNode * re_build_lookahead(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    new_node->expr = (RegExpr *)LookaheadExpr_new(reb->mgr, (RegExpr *)((LexreNode *)node)->expr, node->children[2]->rule->id == EQUALS);
    return (ASTNode *)new_node;
}

ASTNode * re_build_lookbehind(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    
    unsigned char sym_len = 0;
    ASTNode ** elements = node->children[4]->children;
    for (size_t i = 0; i < node->children[4]->nchildren; i++) {
        if (elements[i]->rule->id == NON_RPAREN) {
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
        if (elements[i]->rule->id == NON_RPAREN) {
            sym[j++] = elements[i]->token_start->string[0];
        } else if (elements[i]->children[0]->rule->id == SIMPLE_ESCAPE) {
            get_simple_escape_character(elements[i]->children[0], sym + j++);
        } else { // unicode
            unsigned char unic_len = 0;
            get_unicode(elements[i]->children[0], sym + j, &unic_len);
            j += unic_len;
        }
    }
    return (ASTNode *)new_node;
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

    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));
    ASTNode * rep = node->children[1]->children[0];
    unsigned int min_rep = 0;
    unsigned int max_rep = 0;
    switch (rep->rule->id) {
        case PLUS: {
            min_rep = 1;
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
    new_node->expr = (RegExpr *)RepExpr_new(reb->mgr, ((LexreNode *)node->children[0])->expr, min_rep, max_rep);
    return (ASTNode *)new_node;
}

ASTNode * re_build_sequence(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 1) {
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    }
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));

    RegExpr ** subexprs = MemPoolManager_aligned_alloc(reb->mgr, sizeof(RegExpr *) * node->nchildren, sizeof(RegExpr *));
    for (size_t i = 0; i < node->nchildren; i++) {
        subexprs[i] = ((LexreNode *)node->children[i])->expr;
    }

    new_node->expr = (RegExpr *)SeqExpr_new(reb->mgr, subexprs, node->nchildren);
    return (ASTNode *)new_node;
}

ASTNode * re_build_choice(Production * prod, Parser * parser, ASTNode * node) {
    if (node->nchildren == 1) {
        node->children[0]->rule = (Rule *)prod;
        return node->children[0];
    }
    unsigned int nsub = (node->nchildren + 1) >> 1;
    LexreBuilder * reb = (LexreBuilder *)parser;
    LexreNode * new_node = (LexreNode *)Parser_add_node(parser, (Rule *)prod, node->token_start, node->token_end, node->str_length, 0, sizeof(LexreNode));

    RegExpr ** subexprs = MemPoolManager_aligned_alloc(reb->mgr, sizeof(RegExpr *) * nsub, sizeof(RegExpr *));
    size_t j = 0;
    for (size_t i = 0; i < node->nchildren; i += 2) {
        subexprs[j++] = ((LexreNode *)node->children[i])->expr;
    }

    new_node->expr = (RegExpr *)ChoiceExpr_new(reb->mgr, subexprs, nsub);
    return (ASTNode *)new_node;
}

ASTNode * re_build_lexre(Production * prod, Parser * parser, ASTNode * node) {
    LexreBuilder * reb = (LexreBuilder *)parser;
    reb->entry = ((LexreNode *)node)->expr;
    return node;
}

