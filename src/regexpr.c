#include <stdbool.h>
#include "regexpr.h"

// singletons
struct Symbol empty_symbol = {
    .expr = {.inter = &(RegExprInterface){.eval = match_empty_symbol}},
    .sym = "",
    .sym_len = 0,
};

struct Symbol eos_symbol = {
    .expr = {.inter = &(RegExprInterface){.eval = match_eos}},
    .sym = "\0eos",
    .sym_len = 4,
};

struct Symbol bos_symbol = {
    .expr = {.inter = &(RegExprInterface){.eval = match_bos}},
    .sym = "\0bos",
    .sym_len = 4,
};

struct Symbol any_symbol = {
    .expr = {.inter = &(RegExprInterface){.eval = match_any}},
    .sym = "\0any",
    .sym_len = 4,
};

struct Symbol any_nonl_symbol = {
    .expr = {.inter = &(RegExprInterface){.eval = match_any_nonl}},
    .sym = "\0any_nonl",
    .sym_len = 9,
};

struct RegExprInterface Symbol_inter = {
    .eval = match_symbol
};

BUILD_ALIGNMENT_STRUCT(Symbol)
Symbol * Symbol_new(MemPoolManager * mgr, char const * sym, unsigned char sym_len) {
    Symbol * out = MemPoolManager_aligned_alloc(mgr, sizeof(Symbol), _Alignof(Symbol));
    out->sym = sym;
    out->sym_len = sym_len;
    ((RegExpr *)out)->inter = &Symbol_inter;
    return out;
}

BUILD_ALIGNMENT_STRUCT(DerivedExpr)

BUILD_ALIGNMENT_STRUCT(ChainedExpr)

struct RegExprInterface RangeExpr_inter = {
    .eval = match_range_expr
};

BUILD_ALIGNMENT_STRUCT(RangeExpr)
RangeExpr * RangeExpr_new(MemPoolManager * mgr, uint32_t low, uint32_t high, unsigned char nbytes) {
    RangeExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(RangeExpr), _Alignof(RangeExpr));
    out->low = low;
    out->high = high;
    out->nbytes = nbytes;
    ((RegExpr *)out)->inter = &RangeExpr_inter;
    return out;
}

struct RegExprInterface SRangeExpr_inter = {
    .eval = match_srange_expr
};

BUILD_ALIGNMENT_STRUCT(SRangeExpr)
SRangeExpr * SRangeExpr_new(MemPoolManager * mgr, char low, char high) {
    SRangeExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(SRangeExpr), _Alignof(SRangeExpr));
    out->low = low;
    out->high = high;
    ((RegExpr *)out)->inter = &SRangeExpr_inter;
    return out;
}

struct RegExprInterface SeqExpr_inter = {
    .eval = match_seq
};

SeqExpr * SeqExpr_new(MemPoolManager * mgr, RegExpr ** subexprs, unsigned int nsubs) {
    SeqExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(SeqExpr), _Alignof(SeqExpr));
    out->nsub = nsubs;
    out->subexprs = subexprs;
    ((RegExpr *)out)->inter = &SeqExpr_inter;
    return out;
}

struct RegExprInterface CharSet_inter = {
    .eval = match_char_set
};

CharSet * CharSet_new(MemPoolManager * mgr, char const * set, unsigned char set_len) {
    CharSet * out = MemPoolManager_aligned_alloc(mgr, sizeof(CharSet), _Alignof(CharSet));
    out->sym = set;
    out->sym_len = set_len;
    ((RegExpr *)out)->inter = &CharSet_inter;
    return out;
}

struct RegExprInterface CharClass_inv_inter = {
    .eval = match_char_class_inv
};

struct RegExprInterface CharClass_inter = {
    .eval = match_choice_expr
};

CharClass * CharClass_new(MemPoolManager * mgr, RegExpr ** subexprs, unsigned int nsubs, _Bool inv) {
    CharClass * out = MemPoolManager_aligned_alloc(mgr, sizeof(CharClass), _Alignof(CharClass));
    out->nsub = nsubs;
    out->subexprs = subexprs;
    if (inv) {
        ((RegExpr *)out)->inter = &CharClass_inv_inter;
    } else {
        ((RegExpr *)out)->inter = &CharClass_inter;
    }
    return out;
}

struct RegExprInterface ChoiceExpr_inter = {
    .eval = match_choice_expr
};

ChoiceExpr * ChoiceExpr_new(MemPoolManager * mgr, RegExpr ** subexprs, unsigned int nsubs) {
    ChoiceExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(ChoiceExpr), _Alignof(ChoiceExpr));
    out->nsub = nsubs;
    out->subexprs = subexprs;
    ((RegExpr *)out)->inter = &ChoiceExpr_inter;
    return out;
}

struct RegExprInterface LookaheadExpr_pos_inter = {
    .eval = match_lookahead_pos
};

struct RegExprInterface LookaheadExpr_neg_inter = {
    .eval = match_lookahead_neg
};

LookaheadExpr * LookaheadExpr_new(MemPoolManager * mgr, RegExpr * subexpr, _Bool pos) {
    LookaheadExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(LookaheadExpr), _Alignof(LookaheadExpr));
    out->subexpr = subexpr;
    if (pos) {
        out->expr.inter = &LookaheadExpr_pos_inter;
    } else {
        ((RegExpr *)out)->inter = &LookaheadExpr_neg_inter;
    }
    return out;
}

struct RegExprInterface LookbehindExpr_pos_inter = {
    .eval = match_lookbehind_pos
};

struct RegExprInterface LookbehindExpr_neg_inter = {
    .eval = match_lookbehind_neg
};

BUILD_ALIGNMENT_STRUCT(LookbehindExpr)
LookbehindExpr * LookbehindExpr_new(MemPoolManager * mgr, Symbol * seq, _Bool pos) {
    LookbehindExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(LookbehindExpr), _Alignof(LookbehindExpr));
    if (pos) {
        ((RegExpr *)out)->inter = &LookbehindExpr_pos_inter;
    } else {
        ((RegExpr *)out)->inter = &LookbehindExpr_neg_inter;
    }
    return out;
}

struct RegExprInterface RepExpr_inter = {
    .eval = match_rep
};

BUILD_ALIGNMENT_STRUCT(RepExpr)
RepExpr * RepExpr_new(MemPoolManager * mgr, RegExpr * subexpr, unsigned int min_rep, unsigned int max_rep) {
    RepExpr * out = MemPoolManager_aligned_alloc(mgr, sizeof(RepExpr), _Alignof(RepExpr));
    out->max_rep = max_rep;
    out->min_rep = min_rep;
    ((DerivedExpr *)out)->subexpr = subexpr;
    ((RegExpr *)out)->inter = &RepExpr_inter;
    return out;
}


// not sure this would ever actually be used
int match_empty_symbol(Lexre * lexre, RegExpr const * regexpr) {
    // does not advance
    return 0;
}

int match_eos(Lexre * lexre, RegExpr const * regexpr) {
    // does not advance
    return !Lexre_remain(lexre);
}

int match_bos(Lexre * lexre, RegExpr const * regexpr) {
    // does not advance
    return !Lexre_tell(lexre);
}

int match_any(Lexre * lexre, RegExpr const * regexpr) {
    if (Lexre_remain(lexre)) {
        Lexre_advance(lexre);
        return 1;
    }
    return 0;
}

int match_any_nonl(Lexre * lexre, RegExpr const * regexpr) {
    if (Lexre_remain(lexre) && Lexre_get_byte(lexre) != '\n') {
        Lexre_advance(lexre);
        return 1;
    }
    return 0;
}

int match_symbol(Lexre * lexre, RegExpr const * regexpr) {
    unsigned char sym_len = ((Symbol *)regexpr)->sym_len;
    if (Lexre_remain(lexre) < sym_len) {
        return 0;
    }
    char const * sym = ((Symbol *)regexpr)->sym;
    char const * string = lexre->string + Lexre_tell(lexre);
    for (unsigned char i = 0; i < sym_len; i++) {
        if (*sym != *string) {
            return 0;
        }
        sym++;
        string++;
    }
    Lexre_advance(lexre, sym_len);
    return 1;
}

int match_range_expr(Lexre * lexre, RegExpr const * regexpr) {
    RangeExpr * range = (RangeExpr *)regexpr;
    if (Lexre_remain(lexre) < range->nbytes) {
        return 0;
    }
    uint32_t val = to_uint32_t(lexre->string + Lexre_tell(lexre), range->nbytes);
    if (val < range->low || val > range->high) {
        return 0;
    }
    Lexre_advance(lexre, range->nbytes);
    return 1;
}

int match_srange_expr(Lexre * lexre, RegExpr const * regexpr) {
    SRangeExpr * range = (SRangeExpr *)regexpr;
    if (!Lexre_remain(lexre)) {
        return 0;
    }
    char val = Lexre_get_byte(lexre);
    if (val < range->low || val > range->high) {
        return 0;
    }
    Lexre_advance(lexre);
    return 1;
}

_Bool is_opt(RegExpr const * regexpr) {
    if (regexpr->inter == &RepExpr_inter) {
        RepExpr * rep = (RepExpr *)regexpr;
        return !rep->min_rep;
    }
    return false;
}

int match_seq(Lexre * lexre, RegExpr const * regexpr) {
    SeqExpr * seq = (SeqExpr *)regexpr;
    size_t start = Lexre_tell(lexre);
    unsigned int i = 0;
    RegExpr ** subs = seq->subexprs;
    while (i < seq->nsub) {
        RegExpr * su = subs[i];
        if (su->inter == &ChoiceExpr_inter) {
            ChoiceExpr * choice = (ChoiceExpr *)su;
            SeqExpr ch_seq = {.expr = seq->expr, .nsub = seq->nsub - i, .subexprs = subs + i};
            unsigned int j = 0;
            while (j < choice->nsub) {
                ch_seq.subexprs[0] = choice->subexprs[j];
                if (ch_seq.expr.inter->eval(lexre, &ch_seq)) {
                    break;
                } else {
                    j++;
                }
            }
            ch_seq.subexprs[0] = choice;
            if (j == choice->nsub) { // choice failed for all possibilities
                Lexre_seek(lexre, start, SEEK_SET);
                return 0;
            }
        } else if (su->inter = &RepExpr_inter) {
            RepExpr * rep = (RepExpr *)su;
            RegExpr * expr = ((DerivedExpr *)rep)->subexpr;
            size_t rep_start = Lexre_tell(lexre);
            unsigned int ct = 0;
            for (; ct < rep->min_rep; ct++) {
                if (!expr->inter->eval(lexre, expr)) { // if we cannot get to min_rep whole sequence
                    Lexre_seek(lexre, start, SEEK_SET);
                    return 0;
                }
            }

            // need a condition for rep being the last element in the sequence

            unsigned int max_rep = rep->max_rep;
            while (!max_rep || ct < max_rep) { // break this into two different loops whether max_rap == 0 or not
                SeqExpr ch_seq = {.expr = seq->expr, .nsub = seq->nsub - i, .subexprs = subs + i};
            }
        } else {
            if (!su->inter->eval(lexre, su)) {
                Lexre_seek(lexre, start, SEEK_SET);
                return 0;
            }
        }
        i++;
    }
    return 1;
}

/*
int match_seq(Lexre * lexre, RegExpr const * regexpr) {
    SeqExpr * seq = (SeqExpr *)regexpr;
    size_t start = Lexre_tell(lexre);
    size_t i = 0;
    while (i < seq->nsub) {
        RegExpr * su = seq->subexprs[i];
        if (!su->inter->eval(lexre, su)) { // failed
            Lexre_seek(lexre, start, SEEK_SET);
            return 0;
        }
        i++;
    }
    return 1;
}
*/

// set of 1-byte chars. This is basically match_choice_expr for single characters
int match_char_set(Lexre * lexre, RegExpr const * regexpr) {
    if (!Lexre_remain(lexre)) {
        return 0;
    }
    unsigned char sym_len = ((Symbol *)regexpr)->sym_len;
    char const * sym = ((Symbol *)regexpr)->sym;
    char ch = Lexre_get_byte(lexre);
    for (unsigned char i = 0; i < sym_len; i++) {
        if (*sym == ch) {
            Lexre_advance(lexre);
            return 1;
        }
        sym++;
    }
    return 0;
}

int match_choice_expr(Lexre * lexre, RegExpr const * regexpr) { 
    ChoiceExpr * choice = (ChoiceExpr *)regexpr;
    for (unsigned int i = 0; i < choice->nsub; i++) {
        RegExpr * su = choice->subexprs[i];
        if (su->inter->eval(lexre, su)) {
            return 1;
        }
    }
    return 0;
}

match_eval * match_char_class = &match_choice_expr;

// only advances one byte
int match_char_class_inv(Lexre * lexre, RegExpr const * regexpr) { 
    ChoiceExpr * choice = (ChoiceExpr *)regexpr;
    for (unsigned int i = 0; i < choice->nsub; i++) {
        RegExpr * su = choice->subexprs[i];
        if (su->inter->eval(lexre, su)) {
            return 0;
        }
    }
    Lexre_advance(lexre);
    return 1;
}

int match_lookahead_pos(Lexre * lexre, RegExpr const * regexpre) {
    LookaheadExpr * look = (LookaheadExpr *)regexpre;
    size_t start = Lexre_tell(lexre);
    if (((DerivedExpr *)look)->subexpr->inter->eval(lexre, ((DerivedExpr *)look)->subexpr)) {
        Lexre_seek(lexre, start, SEEK_SET);
        return 1;
    }
    return 0;
}

int match_lookahead_neg(Lexre * lexre, RegExpr const * regexpre) {
    LookaheadExpr * look = (LookaheadExpr *)regexpre;
    size_t start = Lexre_tell(lexre);
    if (((DerivedExpr *)look)->subexpr->inter->eval(lexre, ((DerivedExpr *)look)->subexpr)) {
        Lexre_seek(lexre, start, SEEK_SET);
        return 0;
    }
    return 1;
}

int match_lookbehind_pos(Lexre * lexre, RegExpr const * regexpre) {
    LookbehindExpr * look = (LookbehindExpr *)regexpre;
    // not enough space to check string without invoking U.B.
    if (Lexre_tell(lexre) < look->seq->sym_len) {
        return 0;
    }
    Lexre backlexre = {
        .cursor = 0,
        .end = -1,
        .flags = lexre->flags,
        .size = look->seq->sym_len,
        .string = lexre->string - look->seq->sym_len
    };
    return ((RegExpr *)look->seq)->inter->eval(&backlexre, ((RegExpr *)look->seq));
}

int match_lookbehind_neg(Lexre * lexre, RegExpr const * regexpre) {
    LookbehindExpr * look = (LookbehindExpr *)regexpre;
    // not enough space to check string without invoking U.B.
    if (Lexre_tell(lexre) < look->seq->sym_len) {
        return 0;
    }
    Lexre backlexre = {
        .cursor = 0,
        .end = -1,
        .flags = lexre->flags,
        .size = look->seq->sym_len,
        .string = lexre->string - look->seq->sym_len
    };
    return !((RegExpr *)look->seq)->inter->eval(&backlexre, ((RegExpr *)look->seq));
}

int match_rep(Lexre * lexre, RegExpr const * regexpr) {
    RepExpr * rep = (RepExpr *)regexpr;
    RegExpr * sub = ((DerivedExpr *)rep)->subexpr;
    size_t start = Lexre_tell(lexre);
    size_t ct = 0;
    if (rep->max_rep) {
        unsigned int max_rep = rep->max_rep;
        while (ct < max_rep && sub->inter->eval(lexre, sub)) {
            ct++;
        }
    } else {
        while (sub->inter->eval(lexre, sub)) {
            ct++;
        }
    }
    if (ct < rep->min_rep) {
        Lexre_seek(lexre, start, SEEK_SET);
        return 0;
    }
    return 1;
}

