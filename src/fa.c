#include <string.h>

#include "peggy/mempool.h"
#include "fa.h"
#include "lexre.h"

int pSymbol_fprint(FILE * stream, Symbol * sym) {
    //return sym->inter->fprint(stream, sym);
    // TODO: to fill in
    return 0;
}

int pSymbol_comp(Symbol * a, Symbol * b) {
    if (a->inter != b->inter) {
        return 1;
    }
    return a->inter->comp(a, b);
}

size_t pSymbol_hash(Symbol * key, size_t hash) {
    return key->inter->hash(key, hash);
}

size_t hash_singleton(Symbol * key, size_t hash_size) {
    return size_t_hash((size_t)(uintptr_t)(void *)key, hash_size);
}

size_t hash_symbol_ascii(Symbol * key, size_t hash_size) {
    return size_t_hash(((SymbolASCII *)key)->sym, hash_size);
}

size_t hash_symbol_unicode(Symbol * key, size_t hash_size) {
    return size_t_hash(((SymbolUnicode *)key)->sym, hash_size);
}

size_t hash_symbol_range_ascii(Symbol * key_, size_t hash_size) {
    SymbolRangeASCII * key = (SymbolRangeASCII *)key_;
    size_t val = 0;
    char * pkey = (char *)&val;
    memcpy(pkey, &key->low, sizeof(key->low));
    memcpy(pkey + sizeof(key->low), &key->high, sizeof(key->high));
    return size_t_hash(val, hash_size);
}

size_t str_hash(char const * key, unsigned int len, size_t hash_size) {
    unsigned long long hash = 5381;
    int c;
    unsigned char * str = (unsigned char *) key;

    while ((c = *str)) {
        hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
        str++;
    }

    return hash % hash_size;
}

size_t hash_symbol_lookaround(Symbol * key_, size_t hash_size) {
    SymbolLookaround * key = (SymbolLookaround *)key_;
    return str_hash(key->sym, key->sym_len, hash_size);
}

int comp_singleton(Symbol * a, Symbol * b) {
    if (a != b) {
        return -1;
    }
    return 0;
}

int comp_symbol_ascii(Symbol * a_, Symbol * b_) {
    SymbolASCII * a = (SymbolASCII *)a_;
    SymbolASCII * b = (SymbolASCII *)b_;

    if (a->sym < b->sym) {
        return 1;
    } else if (a->sym > b->sym) {
        return -1;
    }
    return 0;
}

int comp_symbol_unicode(Symbol * a_, Symbol * b_) {
    SymbolUnicode * a = (SymbolUnicode *)a_;
    SymbolUnicode * b = (SymbolUnicode *)b_;

    if (a->sym < b->sym) {
        return 1;
    } else if (a->sym > b->sym) {
        return -1;
    }
    return 0;
}

int comp_symbol_range_ascii(Symbol * a_, Symbol * b_) {
    SymbolRangeASCII * a = (SymbolRangeASCII *)a_;
    SymbolRangeASCII * b = (SymbolRangeASCII *)b_;

    if (a->low < b->low) {
        return -1;
    } else if (a->low == b->low) {
        if (a->high < b->high) {
            return -1;
        } else if (a->high > b->high) {
            return 1;
        }
        return 0;
    }
    return 1;
}

int comp_symbol_lookaround(Symbol * a_, Symbol * b_) {
    SymbolLookaround * a = (SymbolLookaround *)a_;
    SymbolLookaround * b = (SymbolLookaround *)b_;

    unsigned char min_len = a->sym_len < b->sym_len ? a->sym_len : b->sym_len;

    int status = strncmp(a->sym, b->sym, min_len);
    if (status) {
        return status;
    }
    return a->flags != b->flags;
}

int match_symbol_ascii(Symbol * sym_, Lexre * lexre) {
    SymbolASCII * sym = (SymbolASCII *)sym_;
    if (!Lexre_remain(lexre)) {
        return 0;
    }
    if (Lexre_get_byte(lexre) != sym->sym) {
        return 0;
    }
    Lexre_advance(lexre);
    return 1;
}

int match_symbol_unicode(Symbol * sym_, Lexre * lexre) {
    // TODO: need to fix this
    SymbolUnicode * sym = (SymbolUnicode *)sym_;

    return 0;
}

int match_symbol_range_ascii(Symbol * sym_, Lexre * lexre) {
    SymbolRangeASCII * sym = (SymbolRangeASCII *)sym_;
    if (!Lexre_remain(lexre)) {
        return 0;
    }
    if (Lexre_get_byte(lexre) < sym->low || Lexre_get_byte(lexre) > sym->high) {
        return 0;
    }
    Lexre_advance(lexre);
    return 1;
}

// not sure this would ever actually be used
int match_empty(Symbol * sym, Lexre * lexre) {
    // does not advance
    return 0;
}

int match_any(Symbol * sym, Lexre * lexre) {
    if (Lexre_remain(lexre)) {
        Lexre_advance(lexre);
        return 1;
    }
    return 0;
}

int match_any_nonl(Symbol * sym, Lexre * lexre) {
    if (Lexre_remain(lexre) && Lexre_get_byte(lexre) != '\n') {
        Lexre_advance(lexre);
        return 1;
    }
    return 0;
}

int match_eos(Symbol * sym, Lexre * lexre) {
    // does not advance
    return !Lexre_remain(lexre);
}

int match_bos(Symbol * sym, Lexre * lexre) {
    // does not advance
    return !Lexre_tell(lexre);
}

int match_lookaround(Symbol * sym_, Lexre * lex) {
    SymbolLookaround * sym = (SymbolLookaround *)sym_;

    unsigned char * string = NULL;
    int invert = (0 == (sym->flags & SYMBOL_FLAG_POSITIVE));
    if (!(sym->flags & SYMBOL_FLAG_FORWARD)) { // lookbehind
        if (Lexre_tell(lex) < sym->sym_len) { // not enough to run the comparison
            return invert;
        }
        string = lex->string + Lexre_tell(lex) - sym->sym_len;
    } else { // lookahead
        if (Lexre_remain(lex) < sym->sym_len) { // not enough to run comparison
            return invert;
        }
        string = lex->string + Lexre_tell(lex);
    }
    for (unsigned char i = 0; i < sym->sym_len; i++) {
        if (string[i] != sym->sym[i]) {
            return invert;
        }
    }
    return !invert;
}

Symbol sym_empty_ = {
    .inter = &(SymbolInterface){
        .match = &match_empty,
        .comp = &comp_singleton,
        .hash = &hash_singleton
    },
    .obj = &sym_empty_
};
Symbol * sym_empty = &sym_empty_;

Symbol sym_any_ = {
    .inter = &(SymbolInterface){
        .match = &match_any,
        .comp = &comp_singleton,
        .hash = &hash_singleton
    },
    .obj = &sym_any_
};
Symbol * sym_any = &sym_any_;

Symbol sym_any_nonl_ = {
    .inter = &(SymbolInterface){
        .match = &match_any_nonl,
        .comp = &comp_singleton,
        .hash = &hash_singleton
    },
    .obj = &sym_any_nonl_
};
Symbol * sym_any_nonl = &sym_any_nonl_;

Symbol sym_eos_ = {
    .inter = &(SymbolInterface){
        .match = &match_eos,
        .comp = &comp_singleton,
        .hash = &hash_singleton
    },
    .obj = &sym_eos_
};
Symbol * sym_eos = &sym_eos_;

Symbol sym_bos_ = {
    .inter = &(SymbolInterface){
        .match = &match_bos,
        .comp = &comp_singleton,
        .hash = &hash_singleton
    },
    .obj = &sym_bos_
};
Symbol * sym_bos = &sym_bos_;

SymbolInterface ascii_inter = {
    .hash = hash_symbol_unicode, 
    .comp = comp_symbol_unicode, 
    .match = match_symbol_unicode
};

SymbolASCII * SymbolASCII_new(unsigned char sym, MemPoolManager * mgr) {
    SymbolASCII * out = MemPoolManager_aligned_alloc(mgr, sizeof(SymbolASCII), _Alignof(SymbolASCII));
    *out = (SymbolASCII) {
        .sym = sym,
        .symbol = {
            .inter = &ascii_inter,
            .obj = out
        }
    };

    return out;
}

SymbolInterface unicode_inter = {
    .hash = hash_symbol_unicode, 
    .comp = comp_symbol_unicode, 
    .match = match_symbol_unicode
};

SymbolUnicode * SymbolUnicode_new(uint32_t sym, MemPoolManager * mgr) {
    SymbolUnicode * out = MemPoolManager_aligned_alloc(mgr, sizeof(SymbolUnicode), _Alignof(SymbolUnicode));
    *out = (SymbolUnicode) {
        .sym = sym,
        .symbol = {
            .inter = &unicode_inter,
            .obj = out
        }
    };

    return out;
}

SymbolInterface range_ascii_inter = {
    .hash = hash_symbol_unicode, 
    .comp = comp_symbol_unicode, 
    .match = match_symbol_unicode
};

SymbolRangeASCII * SymbolRangeASCII_new(unsigned char low, unsigned char high, MemPoolManager * mgr) {
    SymbolRangeASCII * out = MemPoolManager_aligned_alloc(mgr, sizeof(SymbolRangeASCII), _Alignof(SymbolRangeASCII));
    *out = (SymbolRangeASCII) {
        .low = low,
        .high = high,
        .symbol = {
            .inter = &range_ascii_inter,
            .obj = out
        }
    };

    return out;
}

SymbolInterface lookaround_inter = {
    .hash = hash_symbol_unicode, 
    .comp = comp_symbol_unicode, 
    .match = match_symbol_unicode
};

SymbolLookaround * SymbolLookaround_new(char const * sym, unsigned char len, unsigned char flags, MemPoolManager * mgr) {
    SymbolLookaround * out = MemPoolManager_aligned_alloc(mgr, sizeof(SymbolLookaround), _Alignof(SymbolLookaround));
    *out = (SymbolLookaround) {
        .sym = sym,
        .sym_len = len,
        .flags = flags,
        .symbol = {
            .inter = &lookaround_inter,
            .obj = out
        }
    };

    return out;
}

