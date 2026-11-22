# MacroMix
Your engine's first sparring partner

# Description
MacroMix is a compact, didactic chess engine by Code Monkey King (CMK),
combining the essential techniques of classical chess programming into
a single CECP-compatible engine.

# Features
 - Compact move generator
 - PV, MVV/LVA, killer & history move ordering
 - Iterative deepening
 - Negamax with alpha-beta pruning
 - Quiescence search
 - Simplified evaluation function
 - Triangular PV
 - Basic Winboard/Xboard (CECP) support.

# Why MacroMix?
- **Fundamentals over complexity:** Carefully implemented basic techniques can outperform more sophisticated optimizations applied blindly.
- **Clarity over complexity:** A simplified evaluation function makes search behavior easier to understand and bugs easier to track down.
- **Built to be reused:** MacroMix is modular and compact. Grab individual parts of its implementation and experiment with them in your own engine.
- **Learn the fundamentals:** See how much playing strength can be squeezed out of a handful of well-understood techniques.
- **A familiar foundation:** Think of it as TSCP-style search, paired with a simplified evaluation function and a compact move generator inspired by micro-Max.

# How strong is it?
Hopefully CCRL testing would get a more precise answer,
but based on my tests it should be at least 1500 Elo

# Games
See **MacroMix-games.pgn**

# How to compile it?
Simly run **make** or the explicit command:
    gcc -O3 MacroMix.c -o MacroMix

# Credits
 - Many thanks to Hamza Inaan for numerous bug fixes and
   making search about 2x faster.