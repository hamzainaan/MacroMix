# MacroMix
Xboard/Winboard Chess Engine by CMK

# Description
MacroMix is a compact, didactic chess engine combining the most
essential techniques of classical chess programming into a single
CECP-compatible engine.

# Features
 - Compact move generator
 - PV, MVV/LVA, killer & history move ordering
 - Iterative deepening
 - Negamax with alpha-beta pruning
 - Quiescence search
 - Simplified evaluation function
 - Texel tuned material, piece-square tables & pawn structure
 - Triangular PV
 - Opening book with 240 lines up to 16 plies
 - Configurable Makefile (engine name, version, author & compiler flags)
 - Basic Winboard/Xboard (CECP) support.

# How strong is it?
Version 1.4 beats TSCP 5.5-4.5

# How to compile it?
Simly run **make** or the explicit command:
    **gcc -O3 MacroMix.c -o MacroMix**
