# lexre
A regex compiler using NFA/DFA conversion to build lexers on data streams

## The DFA building process

The build process starts with the grammar written in the EBNF-like form that the primary parser <b>peggy</b> accepts. The extension of peggy's base parser includes a non-deterministic finite automaton (NFA), which is built on the fly while parsing.

Building the NFA comprises construction of symbols, states, and the transitions between them. When a symbol is encountered, it is interpreted as one of two basic types: Symbols and Ranges. Symbols are single values that must match exactly for a transition to occur. Ranges are dual values among the "range" of which (in numerical representation) an input value must exist for the corresponding transition to occur. The types "uint32_t" are used for unicode code points (all of UTF-8/16/32) while simple "char" are used for ASCII.