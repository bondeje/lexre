#ifndef FA_H
#define FA_H

#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include "reutils.h"

#define SYMBOL_FLAG_POSITIVE 1
#define SYMBOL_FLAG_FORWARD 2

struct SymbolArray {
    Symbol ** symbols;
    int nsymbols;
};

typedef struct SymbolInterface {
    int (*comp)(Symbol * a, Symbol * b);
    size_t (*hash)(Symbol * key, size_t hash_size);
    //int (*fprint)(FILE * stream, Symbol * symbol, );
    int (*match)(Symbol * sym, Lexre * lexre);
} SymbolInterface;

// symbol is an interface
struct Symbol {
    SymbolInterface * inter;
    void * obj;
};

BUILD_ALIGNMENT_STRUCT(Symbol)

typedef struct SymbolASCII {
    Symbol symbol;
    unsigned char sym;
} SymbolASCII;

BUILD_ALIGNMENT_STRUCT(SymbolASCII)

SymbolASCII * SymbolASCII_new(unsigned char sym, MemPoolManager * mgr);

typedef struct SymbolUnicode {
    Symbol symbol;
    uint32_t sym;
} SymbolUnicode;

BUILD_ALIGNMENT_STRUCT(SymbolUnicode)

SymbolUnicode * SymbolUnicode_new(uint32_t sym, MemPoolManager * mgr);

typedef struct SymbolRangeASCII {
    Symbol symbol;
    unsigned char low;
    unsigned char high;
} SymbolRangeASCII;

BUILD_ALIGNMENT_STRUCT(SymbolRangeASCII)

SymbolRangeASCII * SymbolRangeASCII_new(unsigned char low, unsigned char high, MemPoolManager * mgr);

typedef struct SymbolLookaround {
    Symbol symbol;
    char * sym;
    unsigned char sym_len;
    unsigned char flags; // bit 0 - positive/negative; bit 1 - forward/backward
} SymbolLookaround;

BUILD_ALIGNMENT_STRUCT(SymbolLookaround)

// file level
extern struct Symbol * sym_empty;
extern struct Symbol * sym_any;
extern struct Symbol * sym_any_nonl;
extern struct Symbol * sym_eos;
extern struct Symbol * sym_bos;

SymbolASCII * SymbolASCII_new(unsigned char sym, MemPoolManager * mgr);
SymbolUnicode * SymbolUnicode_new(uint32_t sym, MemPoolManager * mgr);
SymbolRangeASCII * SymbolRangeASCII_new(unsigned char low, unsigned char high, MemPoolManager * mgr);
SymbolLookaround * SymbolLookaround_new(char * sym, unsigned char len, unsigned char flags, MemPoolManager * mgr);

int pSymbol_comp(Symbol * a, Symbol * b);
size_t pSymbol_hash(Symbol * key, size_t hash);
int pSymbol_fprint(FILE * stream, Symbol * sym);

#endif

