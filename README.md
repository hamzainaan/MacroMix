# MacroMix
Xboard/Winboard Chess Engine

# Description
MacroMix is a compact, didactic chess engine combining the most
essential techniques of classical chess programming into a single
CECP-compatible engine.

# How strong is it?
| Version | Opponent   | Games | Wins | Losses | Draws | Score | Elo        |
|---------|------------|-------|------|--------|-------|-------|------------|
| 1.5     | TSCP 1.83b | 1000  | 885  | 57     | 58    | 91.4% | +411 ± 35 |

Time control 5+0.05, openings from the MacroMix book played with both colors,
engine books disabled.

# How to compile it?
    git clone https://github.com/maksimKorzh/MacroMix
    cd MacroMix
    make