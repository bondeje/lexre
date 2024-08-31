#ifndef REGEXPR_H
#define REGEXPR_H

#include <stddef.h>
#include <stdio.h>

#include "lexre.h"

// returns 1 for success, 0 for failure
typedef int match_eval(Lexre * lexre, RegExpr const * regexpr);

typedef struct RegExprInterface {
    match_eval * eval;
    int (*fprint)(FILE * stream, RegExpr * expr);
} RegExprInterface;

struct RegExpr {
    struct RegExprInterface * inter;
};

typedef struct Symbol {
    struct RegExpr expr;
    char const * sym;
    unsigned char sym_len;
} Symbol;

Symbol * Symbol_new(MemPoolManager * mgr, char const * sym, unsigned char sym_len);

typedef struct DerivedExpr {
    struct RegExpr expr;
    RegExpr * subexpr;
} DerivedExpr;

typedef struct ChainedExpr {
    struct RegExpr expr;
    RegExpr ** subexprs;
    unsigned int nsub;
} ChainedExpr;

typedef struct RangeExpr {
    struct RegExpr expr; 
    // 32 bit to handle unicode
    uint32_t low; 
    uint32_t high;
    unsigned char nbytes; // bytes in each low and high
} RangeExpr;

RangeExpr * RangeExpr_new(MemPoolManager * mgr, uint32_t low, uint32_t high, unsigned char nbytes);

typedef struct SRangeExpr {
    struct RegExpr expr;
    char low;
    char high;
} SRangeExpr;

SRangeExpr * SRangeExpr_new(MemPoolManager * mgr, char low, char high);

#define SeqExpr ChainedExpr
SeqExpr * SeqExpr_new(MemPoolManager * mgr, RegExpr ** subexprs, unsigned int nsubs);
#define CharSet Symbol
CharSet * CharSet_new(MemPoolManager * mgr, char const * set, unsigned char set_len);
// a chain of Charsets and RangeExprs
#define CharClass ChainedExpr
CharClass * CharClass_new(MemPoolManager * mgr, RegExpr ** subexprs, unsigned int nsubs, _Bool inv);
#define ChoiceExpr ChainedExpr
ChoiceExpr * ChoiceExpr_new(MemPoolManager * mgr, RegExpr ** subexprs, unsigned int nsubs);
#define LookaheadExpr DerivedExpr
LookaheadExpr * LookaheadExpr_new(MemPoolManager * mgr, RegExpr * subexpr, _Bool pos);
// this probably could just be a DerivedExpr, but I am not going to add that generality until I expand to add ChoiceExpr as a possibility
typedef struct LookbehindExpr {
    struct RegExpr expr;
    Symbol * seq; // a sequence of characters that must match exactly
} LookbehindExpr;

LookbehindExpr * LookbehindExpr_new(MemPoolManager * mgr, Symbol * seq, _Bool pos);

typedef struct RepExpr {
    struct DerivedExpr der;
    unsigned int min_rep;
    unsigned int max_rep;
} RepExpr;

RepExpr * RepExpr_new(MemPoolManager * mgr, RegExpr * subexpr, unsigned int min_rep, unsigned int max_rep);

// not sure this would ever actually be used
int match_empty_symbol(Lexre * lexre, RegExpr const * regexpr);

int match_eos(Lexre * lexre, RegExpr const * regexpr);

int match_bos(Lexre * lexre, RegExpr const * regexpr);

int match_any(Lexre * lexre, RegExpr const * regexpr);

int match_any_nonl(Lexre * lexre, RegExpr const * regexpr);

int match_symbol(Lexre * lexre, RegExpr const * regexpr);

int match_range_expr(Lexre * lexre, RegExpr const * regexpr);

int match_srange_expr(Lexre * lexre, RegExpr const * regexpr);

int match_seq(Lexre * lexre, RegExpr const * regexpr);

// set of 1-byte chars. This is basically match_choice_expr for single characters
int match_char_set(Lexre * lexre, RegExpr const * regexpr);

int match_choice_expr(Lexre * lexre, RegExpr const * regexpr);

// match_char_class handled internally

// only advances one byte
int match_char_class_inv(Lexre * lexre, RegExpr const * regexpr);

int match_lookahead_pos(Lexre * lexre, RegExpr const * regexpre);

int match_lookahead_neg(Lexre * lexre, RegExpr const * regexpre);

int match_lookbehind_pos(Lexre * lexre, RegExpr const * regexpre);

int match_lookbehind_neg(Lexre * lexre, RegExpr const * regexpre);

int match_rep(Lexre * lexre, RegExpr const * regexpr);

// singletons
extern struct Symbol empty_symbol;

extern struct Symbol eos_symbol;

extern struct Symbol bos_symbol;

extern struct Symbol any_symbol;

extern struct Symbol any_nonl_symbol;

#endif

