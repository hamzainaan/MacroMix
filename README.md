# MacroMix
Xboard/Winboard Chess Engine by CMK

# Description
MacroMix is a compact, didactic chess engine combining the most
essential techniques of classical chess programming into a single
CECP-compatible engine.

# Features
 - Compact move generator
 - TT, PV, MVV/LVA, killer & history move ordering
 - Iterative deepening
 - Negamax with alpha-beta pruning
 - Principal variation search
 - Null move pruning
 - Late move reductions
 - Reverse futility pruning
 - Check extension
 - Transposition table (~1MB) with Zobrist hashing
 - Repetition & fifty move rule detection
 - Quiescence search with delta pruning
 - Simplified evaluation function
 - Pawn structure: doubled, isolated & passed pawns
 - Texel tuned material, piece-square tables & pawn structure
 - Triangular PV
 - Opening book with 240 lines up to 16 plies
 - Configurable Makefile (engine name, version, author & compiler flags)
 - CLI mode with board flip & game loading
 - Basic Winboard/Xboard (CECP) support.

# How strong is it?
| Version | Opponent   | Games | Wins | Losses | Draws | Score | Elo        |
|---------|------------|-------|------|--------|-------|-------|------------|
| 1.5     | TSCP 1.83b | 1000  | 885  | 57     | 58    | 91.4% | +411 ± 35 |

Time control 5+0.05, openings from the MacroMix book played with both colors,
engine books disabled.

# How to compile it?
Simply run **make**
