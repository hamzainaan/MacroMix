/******************************************************************************/
/*                               MacroMix, v1.4                               */
/*                                                                            */
/*       A mix of most essential and basic chess programming techniques       */
/*          bundled into a single didactic CECP chess engine, by CMK          */
/*                                                                            */
/******************************************************************************/
/*                                  Contents                                  */
/*                                                                            */
/* - Compact move generator inspired by H.G. Muller's micro-Max               */
/* - Negamax search and internal opening book inspired by Tom Kerrigan's TSCP */
/* - Null move pruning inspired by N. Blais' implementation in TSCP           */
/* - Simplified evaluation function proposed by Tomasz Michniewski            */
/* - Simple 32-bit piece only Zobrist hashing                                 */
/* - Fixed size (~1MB) Transposition table                                    */
/* - Principle variation search                                               */
/* - Winboard/Xboard communication protocol                                   */
/*                                                                            */
/******************************************************************************/
/*                                  Headers                                   */
/*                                                                            */
/******************************************************************************/
#include <stdio.h>                                /* Console interaction      */
#include <stdlib.h>                               /* Function abs()           */
#include <string.h>                               /* String parsing           */
#include <signal.h>                               /* Preventing interrupt     */
#include <time.h>                                 /* Random book moves seed   */
#ifdef WIN64                                      /*                          */
  #include "windows.h"                            /* WINDOWS GetTickCount()   */
#else                                             /*                          */
  #include "sys/time.h"                           /* UNIX-like gettimeofday() */
#endif                                            /*                          */
/******************************************************************************/
/*                                Definitions                                 */
/*                                                                            */
/******************************************************************************/
typedef struct {                                  /*                          */
  int src, dst, capSq;                            /* Move & captured squares  */
  int pce, capPce;                                /* Attacker & victim pieces */ 
  int prom;                                       /* Pawn promotion piece     */
  int score;                                      /* Score for move ordering  */
} Move;                                           /*                          */
/******************************************************************************/
typedef struct {                                  /*                          */
  Move moves[256];                                /* Moves in a list          */
  int count;                                      /* Move list length         */
} MList;                                          /*                          */
/******************************************************************************/
typedef struct {                                  /*                          */
  Move move;                                      /* Game move                */
  int side, ep, castle, fifty, hash;              /* Position state snapshot  */
} Game;                                           /*                          */
/******************************************************************************/
typedef struct {                                  /*                          */
    int key;                                      /* Position key             */
    int score;                                    /* Position score           */
    int depth;                                    /* Position depth           */
    int flag;                                     /* Score type               */
    Move move;                                    /* Position move            */
} TTEntry;                                        /*                          */
/******************************************************************************/
FILE *bookFile;                                   /* Opening book file        */
/******************************************************************************/
Game game[1024];                                  /* Game moves history       */
int gamePly;                                      /* Game moves history ply   */
int nodes;                                        /* Number of positions      */ 
int board[128], side, ep, castle, fifty, hash;    /* Board position & state   */
int hashPiece[2][7][120];                         /* Random hash numbers      */
int kingSq[2];                                    /* King squares by color    */
/******************************************************************************/
int offsets[7][8] = {                             /* Piece move offsets       */
  { 0 },                                          /*                          */
  { -16, -1, 16, 1, -17, -15, 17, 15 },           /* King                     */
  { 15, 16, 17 },                                 /* Pawn                     */
  { -33, -31, 33, 31, -18, -14, 18, 14 },         /* Knight                   */
  { -17, -15, 17, 15 },                           /* Bishop                   */
  {-16, -1, 16, 1 },                              /* Rook                     */
  { -16, -1, 16, 1, -17, -15, 17, 15 }            /* Queen                    */
};                                                /*                          */
/******************************************************************************/
int pvLength[64];                                 /* PV length at given ply   */
Move pvTable[64][64];                             /* Triangular PV table      */
int histScores[15][128];                          /* History move ordering    */
Move killMoves[2][64];                            /* Killer move ordering     */
#define TT_SIZE (1 << 16)                         /* Transposition table size */
TTEntry tt[TT_SIZE];                              /* ~1mb transposition table */
/******************************************************************************/
int force, tick, stop, sd = 63, st = 1000;        /* CECP control variables   */
int flip, cli = 1, post = 0, mps, inc;            /*                          */
/******************************************************************************/
int material[] = { /* Piece weights */ 0, 0, 100, 320, 330, 500, 900 };     /**/
int pst[7][128] = {{ 0 },                                                   /**/
  {    /* Kings in the opening */          /* Kings in the endgame */       /**/
    -30,-40,-40,-50,-50,-40,-40,-30,    -50,-40,-30,-20,-20,-30,-40,-50,    /**/
    -30,-40,-40,-50,-50,-40,-40,-30,    -30,-20,-10,  0,  0,-10,-20,-30,    /**/
    -30,-40,-40,-50,-50,-40,-40,-30,    -30,-10, 20, 30, 30, 20,-10,-30,    /**/
    -30,-40,-40,-50,-50,-40,-40,-30,    -30,-10, 30, 40, 40, 30,-10,-30,    /**/
    -20,-30,-30,-40,-40,-30,-30,-20,    -30,-10, 30, 40, 40, 30,-10,-30,    /**/
    -10,-20,-20,-20,-20,-20,-20,-10,    -30,-10, 20, 30, 30, 20,-10,-30,    /**/
     20, 20,  0,  0,  0,  0, 20, 20,    -30,-30,  0,  0,  0,  0,-30,-30,    /**/
     20, 30, 10,  0,  0, 10, 30, 20,    -50,-30,-30,-30,-30,-30,-30,-50     /**/
  },                                                                        /**/
  {    /* Pawns */                                                          /**/
      0,  0,  0,  0,  0,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     50, 50, 50, 50, 50, 50, 50, 50,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     10, 10, 20, 30, 30, 20, 10, 10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      5,  5, 10, 25, 25, 10,  5,  5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0,  0,  0, 20, 20,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      5, -5,-10,  0,  0,-10, -5,  5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      5, 10, 10,-20,-20, 10, 10,  5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0,  0,  0,  0,  0,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {    /* Knights */                                                        /**/
    -50,-40,-30,-30,-30,-30,-40,-50,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -40,-20,  0,  0,  0,  0,-20,-40,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -30,  0, 10, 15, 15, 10,  0,-30,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -30,  5, 15, 20, 20, 15,  5,-30,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -30,  0, 15, 20, 20, 15,  0,-30,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -30,  5, 10, 15, 15, 10,  5,-30,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -40,-20,  0,  5,  5,  0,-20,-40,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -50,-40,-30,-30,-30,-30,-40,-50,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {    /* Bishops */                                                        /**/
    -20,-10,-10,-10,-10,-10,-10,-20,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  0,  0,  0,  0,  0,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  0,  5, 10, 10,  5,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  5,  5, 10, 10,  5,  5,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  0, 10, 10, 10, 10,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10, 10, 10, 10, 10, 10, 10,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  5,  0,  0,  0,  0,  5,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -20,-10,-10,-10,-10,-10,-10,-20,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {    /* Rooks */                                                          /**/
      0,  0,  0,  0,  0,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      5, 10, 10, 10, 10, 10, 10,  5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -5,  0,  0,  0,  0,  0,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -5,  0,  0,  0,  0,  0,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -5,  0,  0,  0,  0,  0,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -5,  0,  0,  0,  0,  0,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -5,  0,  0,  0,  0,  0,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0,  0,  0,  5,  5,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {     /* Queens */                                                        /**/
    -20,-10,-10, -5, -5,-10,-10,-20,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  0,  0,  0,  0,  0,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  0,  5,  5,  5,  5,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -5,  0,  5,  5,  5,  5,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0,  0,  5,  5,  5,  5,  0, -5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  5,  5,  5,  5,  5,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10,  0,  5,  0,  0,  0,  0,-10,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -20,-10,-10, -5, -5,-10,-10,-20,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  }                                                                         /**/
};                                                                          /**/
/******************************************************************************/
/*                                  Functions                                 */
/*                                                                            */
/******************************************************************************/
void genMoves(MList *mList, int caps);            /* Generate moves/captures  */
int attack(int sq, int color);                    /* Square attacked by side  */
int make(Move move);                              /* Make move on board       */
void undo();                                      /* Undo move on board       */
void perft(int d);                                /* Perft testing            */
void think();                                     /* Play engine move         */
void openBook();                                  /* Load opening book        */
int bookMove();                                   /* Make book move           */
void closeBook();                                 /* Close opening book       */
int search(int depth,int ply,                     /* Search all moves         */
           int alpha,int beta, int null);         /*                          */
int quiesce(int ply, int alpha, int beta);        /* Search only captures     */
int eval();                                       /* Evaluate position        */
void pickMove(MList *mList, int mv1);             /* Sort next move in a list */
void scoreMoves(MList *mList, int ply);           /* Score moves in order     */
int reps();                                       /* Find move repetitions    */
int inCheck(int color);                           /* Side to move under check */
void setHash();                                   /* Encode board position    */
void ttStore(int key, int depth, int score,       /* Store move to            */
             int flag, int ply);                  /* transposition table      */
int ttProbe(int key, int depth, int alpha,        /* Retrieve move from       */
            int beta, int *score, int ply);       /* transposition table      */
int getTimeMs();                                  /* Get time in ms           */
void printBoard();                                /* Print board to           */
char *moveStr(Move move);                         /* Get move string          */
char *gameStr();                                  /* Get game moves string    */
void printResult();                               /* Print game result        */
Move parseMove(char *moveStr);                    /* Parse move string        */
void loadGame(char *moves);                       /* Load game from string    */
/******************************************************************************/
/*                               Move generator                               */
/*                                                                            */
/******************************************************************************/
void genMoves(MList *mList, int caps) {           /* Generate moves/captures  */
  if (mList) mList->count = 0;                    /* Reset move count         */
  for (int src = 0; src < 120; src++) {           /* Loop over board squares  */
    int pce = board[src];                         /* Get square contents      */
    if (src & 0x88) { src += 7; continue; }       /* Skip offboard squares    */
    if ((pce & side) == 0) continue;              /* Skip opponent's pieces   */
    int pceType = pce & 7;                        /* Extract piece type       */
    for (int i = 0; i < 8; i++) {                 /* Loop over move offsets   */
      int dir = offsets[pceType][i];              /* Piece move direction     */
      if (dir == 0) break;                        /* Skip placeholders        */
      if (pce == 10) dir *= -1;                   /* White pawns direction    */
      int dst = src + dir;                        /* Calculate target square  */
      while (1) {                                 /* Loop over attack ray     */
        if (dst & 0x88) break;                    /* Skip offboard targets    */
        int capSq = dst;                          /* Capture square           */
        if (pceType == 2 && dst == ep)            /* Pawn lands on en passant */
          capSq ^= 16;                            /* Shift capture square     */
        int capPce = board[capSq];                /* Captured piece           */
        if (capPce & side) break;                 /* No own piece capture     */
        if (pceType == 2) {                       /* Piece is a pawn          */
          if (!((dst - src) & 7) && capPce)       /* Pawn moves only forward  */
            break;                                /* Stop otherwise           */
          if (((dst - src) & 7) &&                /* Pawn captures diagonally */
              capPce == 0) break;                 /* Stop otherwise           */
        } if (pceType == 2 &&                     /* Piece is a pawn          */
          ((dst >> 4) == (side == 8 ? 0 : 7))) {  /* Pawn is promoting        */
          for (int prom = 6; prom > 2; prom--) {  /* Loop over promotoins     */
          if (caps == 0)                          /* Generate all moves       */
          mList->moves[mList->count++] = (Move) { /* Add all promotion        */
            src, dst, capSq, pce, capPce, prom, 0 /* Move contents            */
          }; else if (capPce || prom == 6)        /* Captures & queen promos  */
          mList->moves[mList->count++] = (Move) { /* Add tactical moves       */
            src, dst, capSq, pce, capPce, prom, 0 /* Move contents            */
          };}} else if (caps == 0)                /* Generate all moves       */
          mList->moves[mList->count++] = (Move) { /* Add all moves            */
            src, dst, capSq, pce, capPce, 0, 0    /* Move contents            */
        }; else if (caps && capPce) {             /* Generate only captures   */
          mList->moves[mList->count++] = (Move) { /* Add only captures        */
            src, dst, capSq, pce, capPce, 0, 0    /* Move contents            */
        };} if (pceType == 2) {                   /* Piece is a pawn          */
          if ((src >> 4) !=                       /* 2nd or 7th source rank?  */
             ((side == 8) ? 6 : 1)) break;        /* No double push if not    */
          if ((dst >> 4) !=                       /* 3th or 6th target rank?  */
             ((side == 8) ? 5 : 2)) break;        /* No extra push if not     */
        } else if (pceType == 1) {                /* Piece is a king          */
          if ((dir != 1) && (dir != -1)) break;   /* Castles only sideways    */
          if (dir == 1) {                         /* King side castling       */
            if (side == 8 && (castle & 1) == 0)   /* No white king side       */
              break;                              /* Stop                     */
            if (side == 16 && !(castle & 4))      /* No black king side       */
              break;                              /* Stop                     */
            if (board[src + 1]) break;            /* Stop if piece on F1/F8   */
            if (board[src + 2]) break;            /* Stop if piece on G1/G8   */
            if (attack(src, 24 - side)) break;    /* Stop if king is in check */
            if (attack(src + 1, 24 - side))       /* Square F1/F8 is in check */
              break;                              /* Stop                     */
          } if (dir == -1) {                      /* Queen side castling      */
            if (side == 8 && !(castle & 2))       /* No white queen side      */
              break;                              /* Stop                     */
            if (side == 16 && !(castle & 8))      /* No black queen side      */
              break;                              /* Stop                     */
            if (board[src - 1]) break;            /* Stop if piece on D1/D8   */
            if (board[src - 2]) break;            /* Stop if piece in C1/C8   */
            if (board[src - 3]) break;            /* Stop if piece on B1/B8   */
            if (attack(src, 24 - side)) break;    /* Stop if king is in check */
            if (attack(src - 1, 24 - side))       /* Square D1/D8 is in check */
              break;                              /* Stop                     */
          } if (src != ((side == 8) ? 116: 4))    /* King not on E1/E8        */
            break;                                /* Stop                     */
          if (abs(src-dst) > 1) break;            /* Only 2 squares in castle */
        } if (capPce & (24 - side)) break;        /* Captured opponent piece  */
        if ((pceType & 4) == 0 &&                 /* Piece is not a slider    */
             pceType != 2 &&                      /* Piece is not a pawn      */
             pceType != 1)                        /* Piece is not a king      */
          break;                                  /* No slide along the ray   */
        dst += dir;                               /* Sliders go next square   */
      }                                           /*                          */
    }                                             /*                          */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
int attack(int sq, int color) {                   /* Square attacked by color */
  for (int pceType = 1; pceType < 6; pceType++) { /* Loop over all but queen  */
    int pce = pceType | color;                    /* Build piece              */
    if (pceType == 2) {                           /* Attacked by a pawn       */
      int dir = ((color == 8) ? 16 : -16);        /* Attack direction         */
      for (int cap = -1; cap <= 1; cap += 2) {    /* Pawn attack offsets      */
        int src = sq + dir + cap;                 /* Attack source square     */
        if (!(src & 0x88) && board[src] == pce)   /* Pawn attacks square      */
          return 1;                               /* Return true              */
      }                                           /*                          */
    } else {                                      /* Attacked by a piece      */
      int slider = pceType & 0x04;                /* Slider or leaper piece   */
      for (int d = 0; d < 8; d++) {               /* Piece attack offsets     */
        int dir = offsets[pceType][d];            /* Attack direction         */
        if (dir == 0) break;                      /* Skip placeholders        */
        int src = sq;                             /* Attack source square     */
        do {                                      /* Follow attack ray        */
            src += dir;                           /* Attack source square     */
            if (src & 0x88) break;                /* Skip offboard squares    */
            int att = board[src];                 /* Attacking piece          */
            if (att != 0) {                       /* Not an empty square      */
                if (att == pce ||                 /* Piece attacks square     */
                  (slider && att == (6 | color))) /* or queen along the ray   */
                  return 1;                       /* Return true              */
                break;                            /* Search no more           */
            }                                     /*                          */
        } while (slider);                         /* Repeat for slider pieces */
      }                                           /*                          */
    }                                             /*                          */
  } return 0;                                     /* Square is not attacked   */
}                                                 /*                          */
/******************************************************************************/
int make(Move move) {                             /* Make move on board       */
  game[gamePly].move = move;                      /* Save move into a game    */
  game[gamePly].side = side;                      /* Save side to move        */
  game[gamePly].ep = ep;                          /* Save en passant square   */
  game[gamePly].castle = castle;                  /* Save castling rights     */
  game[gamePly].fifty = fifty;                    /* Save fifty move count    */
  game[gamePly].hash = hash;                      /* Save hash key            */
  gamePly++;                                      /* Increment game ply       */
  board[move.capSq] = 0;                          /* Empty captured square    */
  board[move.src] = 0;                            /* Empty source square      */
  board[move.dst] = move.pce;                     /* Move piece               */
  if ((move.pce & 7) == 1)                        /* Piece is a king          */
    kingSq[side >> 4] = move.dst;                 /* Update king square       */
  ep = -1;                                        /* Reset en passant square  */
  if ((move.pce & 7) == 2) {                      /* Piece is a pawn          */
    int srcRank = move.src >> 4;                  /* Extract source rank      */
    int dstRank = move.dst >> 4;                  /* Extract target rank      */
    if (                                          /* Double pawn push         */
      (srcRank == ((side == 8) ? 6 : 1)) &&       /* Source rank is 2nd / 7th */
      (dstRank == ((side == 8) ? 4 : 3))          /* Target rank is 4th / 5th */
    ) ep = move.dst ^ 16;                         /* Set en passant square    */
    if ((move.dst >> 4) == (side == 8 ? 0 : 7)) { /* Pawn promotion           */
      board[move.dst] = move.prom | side;         /* Promote to given piece   */
    }                                             /*                          */
  } if ((move.pce & 7) == 1 &&                    /* Piece is a king          */
     ((move.src == 116) || (move.src == 4))) {    /* Castling move            */
    switch(move.dst) {                            /* Where king castled       */
      case 118:                                   /* Square G1                */
        board[117] = 13; board[119] = 0; break;   /* Move rook from H1 to F1  */
      case 114:                                   /* Square C1                */
        board[115] = 13; board[112] = 0; break;   /* Move rook from A1 to D1  */
      case 6:                                     /* Square G8                */
        board[5] = 21; board[7] = 0; break;       /* Move rook from H8 to F8  */
      case 2:                                     /* Square C8                */
        board[3] = 21; board[0] = 0; break;       /* Move rook from A8 to D8  */
    }                                             /*                          */
  } castle &= board[move.src + 8];                /* Update castling source   */
  castle &= board[move.dst + 8];                  /* Update castling target   */
  if (move.capPce || (move.pce & 7) == 2)         /* On capture or pawn move  */
    fifty = 0;                                    /* Reset fifty count        */
  else fifty++;                                   /* Increment fifty move     */
  side = 24 - side;                               /* Swap the side to move    */
  if (inCheck(24 - side)) {                       /* King in check after move */
    undo();                                       /* Take move back           */
    return 0;                                     /* Return illegal move      */
  }                                               /*                          */
  return 1;                                       /* Return legal move        */
}                                                 /*                          */
/******************************************************************************/
void undo() {                                     /* Undo move on board       */
  gamePly--;                                      /* Decrement game ply       */
  Move move = game[gamePly].move;                 /* Load move from a game    */
  side = game[gamePly].side;                      /* Load side to move        */
  ep = game[gamePly].ep;                          /* Load en passant square   */
  castle = game[gamePly].castle;                  /* Load castling rights     */
  fifty = game[gamePly].fifty;                    /* Load fifty move count    */
  hash = game[gamePly].hash;                      /* Load hash key            */
  board[move.dst] = 0;                            /* Empty target square      */
  board[move.src] = move.pce;                     /* Move a piece back        */
  board[move.capSq] = move.capPce;                /* Restore captured piece   */
  if ((move.pce & 7) == 1)                        /* Piece is a king          */
    kingSq[side >> 4] = move.src;                 /* Restore king square      */
  if ((move.pce & 7) == 1 &&                      /* Piece is a king          */
     ((move.src == 116) || (move.src == 4))) {    /* Castling move            */
    switch(move.dst) {                            /* Where king castled       */
      case 118:                                   /* Square G1                */
        board[119] = 13; board[117] = 0; break;   /* Move rook from F1 to H1  */
      case 114:                                   /* Square C1                */
        board[112] = 13; board[115] = 0; break;   /* Move rook from D1 to A1  */
      case 6:                                     /* Square G8                */
        board[7] = 21; board[5] = 0; break;       /* Move rook from F8 to H8  */
      case 2:                                     /* Square C8                */
        board[0] = 21; board[3] = 0; break;       /* Move rook from D8 to A8  */
    }                                             /*                          */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
/*                                   Perft                                    */
/*                                                                            */
/******************************************************************************/
void perft(int depth) {                           /* Move generator test      */
  if (depth == 0) { nodes++; return; }            /* Count leaf nodes         */
  MList mList; genMoves(&mList, 0);               /* Generate moves           */
  for (int i = 0; i < mList.count; i++) {         /* Loop over moves          */
    Move move = mList.moves[i];                   /* Get current move         */
    if (make(move) == 0) continue;                /* Make move, skip illegals */
    perft(depth - 1);                             /* Traverse move tree       */
    undo();                                       /* Take move back           */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
/*                                Opening book                                */
/*                                                                            */
/******************************************************************************/
void openBook() {                                 /* Open book file           */
  bookFile = fopen("book.txt", "r");              /* Read book file stream    */
  if (bookFile == 0)                              /* No book file             */
    printf("Opening book missing\n");             /* Inform user              */
}                                                 /*                          */
/******************************************************************************/
int bookMove() {                                  /* Make book move if any    */
  MList bookMoves;                                /* Create move list         */
  bookMoves.count = 0;                            /* Reset move count         */
  char bookLine[2048];                            /* Book line storage        */
  char *gameLine = gameStr();                     /* Get game moves           */
  if (bookFile != NULL) {                         /* Skip if no book file     */
    fseek(bookFile, 0, SEEK_SET);                 /* Seek through book file   */
    while (fgets(bookLine, 2048, bookFile)) {     /* Read book line from file */
      int match = 1;                              /* Match line flag          */
      for (int i = 0; i<strlen(gameLine); i++) {  /* Compare game line to     */
        if (bookLine[i] == '\0' ||                /* each line                */
            bookLine[i] != gameLine[i]) {         /* in the opening book      */
          match = 0; break;                       /* End of book or mismatch  */
        }                                         /*                          */
      } if (match) {                              /* Game matches book line   */
        char *next = &bookLine[strlen(gameLine)]; /* Extract next book move   */
        Move move = parseMove(next);              /* Parse book move          */
        bookMoves.moves[bookMoves.count] = move;  /* Add book move to list    */
        bookMoves.count++;                        /* Increment move counter   */
      }                                           /*                          */
    }                                             /*                          */
  } if (bookMoves.count == 0) return 0;           /* No book move found       */
  srand(time(NULL));                              /* Set random seed          */
  int rIndex = rand() % bookMoves.count;          /* Pick random move index   */
  Move bestMove = bookMoves.moves[rIndex];        /* Pick random book move    */
  make(bestMove);                                 /* Play book move on board  */
  if (cli) printf("Book ");                       /* Print "Book" in CLI mode */
  printf("move %s", moveStr(bestMove));           /* CECP move output         */
  printf("\n");                                   /* Print best move          */
  if (cli) { printResult(); printBoard(); }       /* Print board in CLI mode  */
  fflush(stdout);                                 /* Flush output             */
  return 1;                                       /* Found book move          */
}                                                 /*                          */
/******************************************************************************/
void closeBook() {                                /* Close book file          */
  if (bookFile)                                   /* Book file stream exists  */
    fclose(bookFile);                             /* Close book file stream   */
  bookFile = NULL;                                /* Free memory              */
}                                                 /*                          */
/******************************************************************************/
/*                                   Search                                   */
/*                                                                            */
/******************************************************************************/
void think() {                                    /* Engine move              */
  if (bookMove()) return;                         /* Make book move if any    */
  tick = getTimeMs();                             /* Start timer              */
  stop = 0;                                       /* Reset stop flag          */
  nodes = 0;                                      /* Reset position count     */
  memset(pvTable,0,sizeof(pvTable));              /* Reset PV table           */
  memset(pvLength,0,sizeof(pvLength));            /* Reset PV length table    */
  memset(killMoves,0,sizeof(killMoves));          /* Reset killers table      */
  memset(histScores,0,sizeof(histScores));        /* Reset history table      */
  Move bestMove = { 0 };                          /* Init PV move holder      */
  for (int d = 1; d <= sd; d++) {                 /* Iterative deepening      */
    int s = search(d, 0, -50000, 50000, 1);       /* Search best move         */
    if (stop) break;                              /* Time is up or game over  */
    bestMove = pvTable[0][0];                     /* Record last best move    */
    if (post) { printf("%d %d %d %d ",            /* Engine search output     */
      d, s, (getTimeMs()-tick)/10, nodes);        /* depth/score/time/nodes   */
    for (int i = 0; i < pvLength[0]; i++) {       /* Loop over PV line        */
      Move move = pvTable[0][i];                  /* Get PV move              */
      printf("%s", moveStr(move));                /* Print PV move            */
      if (i < pvLength[0] - 1)                    /* Not the last PV move     */
        printf(" ");                              /* Print separator          */
    } printf("\n");                               /* Print new line           */
    } if (abs(s) > 48000) break;                  /* Stop if found mate       */
  } if (bestMove.pce == 0) return;                /* No legal move to play    */
  make(bestMove); printf("move ");                /* Make best last PV move   */
  printf("%s", moveStr(bestMove)); printf("\n");  /* Print best move          */
  if (cli) { printResult(); printBoard(); }       /* Print board in CLI mode  */
  fflush(stdout);                                 /* Flush output             */
}                                                 /*                          */
/******************************************************************************/
int search(int depth, int ply,                    /* Negamax search           */
           int alpha, int beta, int null) {       /*                          */
  if (stop) return 0;                             /* Time is up, return       */
  int oldAlpha = alpha;                           /* Previous best score      */
  int legalMoves = 0;                             /* Legal moves counter      */
  int score = 0;                                  /* Move score               */
  if (depth == 0)                                 /* Main search is done      */
    return quiesce(ply, alpha, beta);             /* Do quiescence search     */
  nodes++;                                        /* Count searched positions */
  if ((nodes & 1023) == 0)                        /* Check time control       */
    if ((getTimeMs() - tick) >= st) stop = 1;     /* Time is up, set stop     */
  pvLength[ply] = ply;                            /* Init PV length           */
  setHash();                                      /* Update position hash     */
  if (ply && reps()) return 0;                    /* 3 Fold repetition, draw  */
  int pvNode = (beta - alpha) > 1;                /* Distinguish PV nodes     */
  int key  = hash ^ (side == 8 ?                  /* Get position key         */
             0xAAAAAAAAu : 0x55555555u);          /* relative to side to move */                                
  int ttHit = ttProbe(key, depth, alpha, beta,    /* Read TT entry            */
                                 &score, ply);    /*                          */
  if (ply && ttHit && pvNode == 0) return score;  /* Return TT score          */
  if (ply >= 62) return eval();                   /* Too deep, evaluate       */
  if (gamePly >= 1022) return eval();             /* Too deep, evaluate       */
  int check = inCheck(side);                      /* King in check flag       */
  if (check) depth++;                             /* King in check, go deeper */  
  if (check == 0 && ply && null) {                /* Null move pruning        */
    int matScore = 0;                             /* Material score           */
    for (int sq = 0; sq < 120; sq++) {            /* Loop over board squares  */
      if (sq & 0x88) { sq += 7; continue; }       /* Skip offboard square     */
      int pce = board[sq], pceType = pce & 7;     /* Get piece. extract type  */
      if (pce && pceType != 1 && (pce & side)) {  /* Pieces of side to move   */
        matScore += material[pceType];            /* Count all but pawns      */
      }                                           /*                          */
    } int rDepth = (matScore > 1500) ? 2 : 1;     /* Calculat reduce depth    */
    if (depth > rDepth && matScore > 1300) {      /* Null move threshold      */
      int oldSide = side, oldEp = ep;             /*                          */
      int oldCastle = castle, oldHash = hash;     /* Preserve position state  */
      int oldFifty = fifty;                       /*                          */
      side = 24 - side;                           /* Swap the side            */
      score = -search(depth - 1 - rDepth,         /* Use reduced depth        */
                   ply+1,-beta,-beta+1, 0);       /* Negamax search           */
      side = oldSide, ep = oldEp;                 /*                          */
      castle = oldCastle, hash = oldHash;         /* Restore position state   */
      fifty = oldFifty;                           /*                          */
      if (stop) return 0;                         /* Time is up, return       */
      if (score >= beta) return beta;             /* Beta cutoff              */
    }                                             /*                          */
  } MList mList; genMoves(&mList, 0);             /* Generate moves           */
  scoreMoves(&mList, ply);                        /* Score moves              */
  for (int i = 0; i < mList.count; i++) {         /* Loop over move list      */  
    pickMove(&mList, i);                          /* Bring best move forward  */
    Move move = mList.moves[i];                   /* Get current move         */
    if (make(move) == 0) continue;                /* Make move, skip illegals */
    if (legalMoves == 0)                          /* First move               */
      score = -search(depth-1, ply+1,             /* Full window search       */
                      -beta, -alpha, 1);          /*                          */
    else {                                        /* Other moves              */
        score = -search(depth-1, ply+1,           /* PVS null window search   */
                        -alpha-1, -alpha, 1);     /*                          */
        if (score > alpha && score < beta)        /* Found a better move      */
            score = -search(depth-1, ply+1,       /* Full window re-search    */
                            -beta, -alpha, 1);    /*                          */
    } undo();                                     /* Take move back           */
    legalMoves++;                                 /* Count legal moves        */
    if (stop) return 0;                           /* Time is up, return       */
    if (score > alpha) {                          /* Found a better move      */
      if (move.capPce == 0)                       /* Quiet move               */
        histScores[board[move.src] & 15]          /* Update history scores    */
          [move.dst] += depth;                    /* By depth count           */
      if (score >= beta) {                        /* Found a killer move      */
        if (move.capPce == 0) {                   /* Quiet move               */
          killMoves[1][ply] = killMoves[0][ply];  /* Update 2nd killer move   */
          killMoves[0][ply] = move;               /* Update 1st killer move   */
        } return beta;                            /* Beta cutoff              */
      } alpha = score;                            /* Update alpha score       */
      pvTable[ply][ply] = move;                   /* Record PV table entry    */
      for (int n = ply + 1;                       /* Copy remaining PV moves  */
               n < pvLength[ply + 1]; n++)        /* From the next ply        */
        pvTable[ply][n] = pvTable[ply + 1][n];    /* To current PV table ply  */
      pvLength[ply] = pvLength[ply + 1];          /* Update PV length         */
    }                                             /*                          */
  } if (legalMoves == 0) {                        /* No legal moves available */
    if (check) return -49000 + ply;               /* In check, checkmate      */
    else return 0;                                /* Not in check, stalemate  */
  } if (fifty >= 100) return 0;                   /* Fifty move draw          */
  int flag = 0;                                   /* Hash exact score flag    */
  if (alpha <= oldAlpha) flag = 1;                /* Hash alpha score flag    */
  else if (alpha >= beta) flag = 2;               /* Hash beta score flag     */
  ttStore(key, depth, alpha, flag, ply);          /* Write TT entry           */
  return alpha;                                   /* Return best score        */
}                                                 /*                          */
/******************************************************************************/
int quiesce(int ply, int alpha, int beta) {       /* Quiescence search        */
  if (stop) return 0;                             /* Time is up, return       */
  nodes++;                                        /* Count searched positions */
  if ((nodes & 1023) == 0)                        /* Check time control       */
    if ((getTimeMs() - tick) >= st) stop = 1;     /* Time is up, set stop     */
  pvLength[ply] = ply;                            /* Init PV length           */
  if (ply >= 62) return eval();                   /* Too deep, evaluate       */
  if (gamePly >= 1022) return eval();             /* Too deep, evaluate       */
  int score = eval();                             /* Evaluate position        */
  if (score >= beta) return beta;                 /* Beta cutoff              */
  if (score > alpha) alpha = score;               /* Update alpha score       */
  MList mList; genMoves(&mList, 1);               /* Generate moves           */
  scoreMoves(&mList, ply);                        /* Score moves              */
  for (int i = 0; i < mList.count; i++) {         /* Loop over move list      */  
    pickMove(&mList, i);                          /* Bring best move forward  */
    Move move = mList.moves[i];                   /* Get current move         */
    if (make(move) == 0) continue;                /* Make move, skip illegals */
    int score = -quiesce(ply + 1, -beta, -alpha); /* Quiescence search        */
    undo();                                       /* Take move back           */
    if (stop) return 0;                           /* Time is up, return       */
    if (score > alpha) {                          /* Found a better capture   */
      if (score >= beta) return beta;             /* Beta cutoff              */
      alpha = score;                              /* Update alpha score       */
      pvTable[ply][ply] = move;                   /* Record PV table entry    */
      for (int n = ply + 1;                       /* Copy remaining PV moves  */
               n < pvLength[ply + 1]; n++)        /* From the next ply        */
        pvTable[ply][n] = pvTable[ply + 1][n];    /* To current PV table ply  */
      pvLength[ply] = pvLength[ply + 1];          /* Update PV length         */
    }                                             /*                          */
  } return alpha;                                 /* Return best score        */
}                                                 /*                          */
/******************************************************************************/
int eval() {                                      /* Static evaluation        */
  int score = 0;                                  /* Reset static score       */
  int matWhite = 0, matBlack = 0;                 /* White & black material   */
  int kingWhite = 0, kingBlack = 0;               /* King squares             */
  for (int sq = 0; sq < 120; sq++) {              /* Loop over board squares  */
    if (sq & 0x88) { sq += 7; continue; }         /* Skip offboard square     */
    int pce = board[sq];                          /* Get square contents      */
    int pceType = pce & 7;                        /* Extract piece type       */
    if (pceType == 2) {                           /* Pawn structure scoring   */
        int oPawn = pce ^ 24;                     /* Enemy pawn               */
        int dir = (pce == 10) ? -16 : 16;         /* Pawn move direction      */
        int file = sq & 7;                        /* Pawn file                */
        int rank = sq >> 4;                       /* Pawn rank                */
        int isolated = 1;                         /* Isolated pawn flag       */
        int passed = 1;                           /* Passed pawn flag         */
        int doubled = 0;                          /* Doubled pawn flag        */
        for (int r = 0; r < 8; r++) {             /* Loop over adjacent file  */
            if (file > 0 &&                       /* Not A-file               */
              board[r * 16 + file - 1] == pce)    /* Friendly pawn on left    */
              isolated = 0;                       /* Pawn is not isolated     */
            if (file < 7 &&                       /* Not H-file               */
              board[r * 16 + file + 1] == pce)    /* Friendly pawn on right   */
              isolated = 0;                       /* Pawn is not isolated     */
        } for (int r = rank + (dir < 0 ? -1 : 1); /* Check pawns ahead        */
             r >= 0 && r < 8;                     /* on same and              */
             r += (dir < 0 ? -1 : 1)) {           /* adjacent files           */
            if (board[r * 16 + file] == oPawn)    /* Enemy pawn on same file  */
              passed = 0;                         /* Pawn is not passed       */
            if (board[r * 16 + file] == pce)      /* Own pawn on same file    */
              doubled = 1;                        /* Pawn is doubled          */
            if (file > 0 &&                       /* Not A-file               */
              board[r * 16 + file - 1] == oPawn)  /* Enemy pawn left          */
              passed = 0;                         /* Pawn is not passed       */
            if (file < 7 &&                       /* Not H-file               */
              board[r * 16 + file + 1] == oPawn)  /* Enemy pawn right         */
              passed = 0;                         /* Pawn is not passed       */
        } if (doubled) {                          /* Pawn is doubled          */
            if (pce == 10) score -= 20;           /* Give penalty to white    */
            else score += 20;                     /* Give penalty to black    */
        } if (isolated) {                         /* Pawn is isolated         */
            if (pce == 10) score -= 20;           /* Give penalty to white    */
            else score += 20;                     /* Give penalty to black    */
        } if (passed) {                           /* Pawn is passed           */
            int adv = (pce == 10) ?               /* For both colors          */
                6 - rank : rank - 1;              /* Calculate pawn rank      */
            int bonus = 30 + adv * adv * 5;       /* Passed pawn bonus        */
            if (pce == 10) score += bonus;        /* Give bonus to white      */
            else score -= bonus;                  /* Give bonus to black      */
        }                                         /*                          */
    } if (pce == 9) kingWhite = sq;               /* Remember white king      */
    if (pce == 17) kingBlack = sq;                /* Remember black king      */
    if (pce & 8) matWhite += material[pceType];   /* Count white material     */
    if (pce & 16) matBlack += material[pceType];  /* Count black material     */
    if ((pce & 8) && pceType != 1)                /* White piece but king     */
      score += pst[pceType][sq];                  /* White positional score   */
    if ((pce & 16) && pceType != 1)               /* Black piece but king     */
      score -= pst[pceType][sq ^ 112];            /* Black positional score   */
  } if (matWhite < 1350) kingWhite += 8;          /* White king in an endgame */
  if (matBlack < 1350) kingBlack += 8;            /* Black king in an endgame */
  score += matWhite + pst[1][kingWhite];          /* White material and king  */
  score -= matBlack + pst[1][kingBlack ^ 112];    /* Black material and king  */
  return (side == 8) ? score : -score;            /* Side respective score    */
}                                                 /*                          */
/******************************************************************************/
void pickMove(MList *mList, int mv1) {            /* Pick best remaining move */
  int count = mList->count;                       /* Move list length         */
  for (int mv2 = mv1 + 1; mv2 < count; mv2++) {   /* Loop over next moves     */
    if (mList->moves[mv1].score <                 /* Current score            */
        mList->moves[mv2].score) {                /* Less than next score     */
      Move temp = mList->moves[mv1];              /* Create temp move         */
      mList->moves[mv1] = mList->moves[mv2];      /* Higher score goes first  */
      mList->moves[mv2] = temp;                   /* Lower score goes after   */
    }                                             /*                          */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
void scoreMoves(MList *mList, int ply) {          /* Move scoring             */
  for (int mv = 0; mv < mList->count; mv++) {     /* Loop over moves          */
    Move *move = &mList->moves[mv];               /* Get current move         */
    int pvSrc = pvTable[0][ply].src;              /* Get PV move source       */
    int pvDst = pvTable[0][ply].dst;              /* Get PV move target       */
    if (move->src == pvSrc && move->dst == pvDst) /* Move is a PV move        */
      move->score = 2000000000;                   /* Give it highest score    */
    else if (move->capPce) {                      /* Capture move             */
      int att = board[move->src] & 7;             /* Get attacker piece       */
      int vic = move->capPce & 7;                 /* Get victim piece         */
      move->score = vic*100 - att + 1000000000;   /* MVV/LVA captures sort    */
    } else {                                      /* Quiet move               */
      if (move->src == killMoves[0][ply].src &&   /*                          */
          move->dst == killMoves[0][ply].dst)     /* Move is 1st killer move  */
        move->score = 900000000;                  /* Score 1st killer move    */
      else if (killMoves[1][ply].src == move->src /*                          */
           && killMoves[1][ply].dst == move->dst) /* Move is 2nd killer move  */
        move->score = 800000000;                  /* Score 2nd killer move    */
      else move->score = histScores[              /* Other quiet moves        */
        board[move->src] & 15][move->dst] + 7000; /* Assign history score     */
    }                                             /*                          */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
int reps() {                                      /* Repetition detection     */
  int reps = 0;                                   /* Position repetitions     */
  for (int i = gamePly - 2;                       /* Loop over game history   */
       i >= gamePly - fifty; i -= 2)              /* Same side to move only   */
    if (game[i].hash == hash) reps++;             /* Found repetition         */
  return reps;                                    /* Times position repeats   */
}                                                 /*                          */
/******************************************************************************/
int inCheck(int color) {                          /* Is king in check ?       */
  return attack(kingSq[color >> 4], 24 - color);  /* King attacked by enemy?  */
}                                                 /*                          */
/******************************************************************************/
void setHash() {                                  /* Encode board position    */
  hash = 0;                                       /* Reset position hash key  */
  for (int sq = 0; sq < 120; sq++) {              /* Loop over board squares  */
    if (sq & 0x88) { sq += 7; continue; }         /* Skip offboard squares    */
    int pce = board[sq];                          /* Get square contents      */
    int col = -1;                                 /* Set no piece color       */
    if (pce & 8) col = 0;                         /* Piece is white           */
    if (pce & 16) col = 1;                        /* Piece is black           */
    if (col >= 0)                                 /* Piece is not empty       */
      hash ^= hashPiece[col][pce & 7][sq];        /* Hash piece               */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
void ttStore(int key, int depth, int score,       /* Write entry to TT        */
                      int flag, int ply) {        /*                          */
    TTEntry *e = &tt[key & (TT_SIZE - 1)];        /* Create TT entry          */
    if (depth >= e->depth || e->key != key) {     /* Replace old entry        */
        e->key = key;                             /* Write position key       */
        if (score > 48000) score += ply;          /* Normalize winning score  */
        else if (score < -48000) score -= ply;    /* Normalize loosing score  */
        e->score = score;                         /* Write score              */
        e->depth = depth;                         /* Write depth              */
        e->flag = flag;                           /* Write score type flag    */
    }                                             /*                          */
}                                                 /*                          */
/******************************************************************************/
int ttProbe(int key, int depth, int alpha,        /* Read entry from TT       */
            int beta, int *score, int ply) {      /*                          */
    TTEntry *e = &tt[key & (TT_SIZE - 1)];        /* Create TT entry          */
    if (e->key != key) return 0;                  /* Skip on key mismatch     */
    if (e->depth < depth) return 0;               /* Skip on depth mismatch   */
    int ttScore = e->score;                       /* Init TT score            */
    if (ttScore > 48000) ttScore -= ply;          /* Normalize winning score  */
    else if (ttScore < -48000) ttScore += ply;    /* Normalize loosing score  */
    if (e->flag == 0 ||                           /* Exact score flag match   */
        (e->flag == 1 && ttScore <= alpha) ||     /* Alpha score flag match   */
        (e->flag == 2 && ttScore >= beta)) {      /* Beta score flag match    */
        *score = ttScore;                         /* Assign return score      */
        return 1;                                 /* Return TT hit            */
    } return 0;                                   /* Return TT miss           */
}                                                 /*                          */
/******************************************************************************/
int getTimeMs() {                                 /* Count time spent         */
  #ifdef WIN64                                    /* On Windows               */
    return GetTickCount ();                       /* Use GetTickCount()       */
  #else                                           /* On Linux                 */
    struct timeval t;                             /* Create time structure    */
    gettimeofday (&t, NULL);                      /* Use gettimeofday()       */
    return t.tv_sec*1000 + t.tv_usec/1000;        /* return time in ms        */
  #endif	                                      /*                          */
}                                                 /*                          */
/******************************************************************************/
/*                                   Output                                   */
/*                                                                            */
/******************************************************************************/
void printBoard() {                               /* Print board to console   */
  #ifdef WIN64                                    /* Windows ASCII version    */
    printf("\n");                                 /* Print new line           */
    for (int row = 0; row < 8; row++) {           /* Loop over board rows     */
      for (int col = 0; col < 16; col++) {        /* Loop over board cols     */
        if (col == 0)                             /* On board edge            */
        printf(" %d  ", flip ? row + 1:8 - row);  /* Print row number         */
        int file, rank;                           /* Flip file & rank holders */
        if (flip) {                               /* Black's perspective view */
          file = 8 - 1 - col;                     /* Mirror file              */
          rank = 8 - 1 - row;                     /* Mirror rank              */
        } else { file = col; rank = row; }        /* White's perspective view */
        int sq = rank * 16 + file;                /* Calculate square index   */
        if (sq & 0x88) continue;                  /* Skip offboard squares    */
        char p[] = ".kpnbrq--KPNBRQ";             /* ASCII piece characters   */
        printf("%c ", p[board[sq] & 15]);         /* Print piece              */
      } printf("\n");                             /* Print new line           */
    } if (flip == 0)                              /* Print files              */
    printf("\n    a b c d e f g h\n\n");          /* White's perspective view */
    else printf("\n    h g f e d c b a\n\n");     /* Black's perspective view */
  #else                                           /* Linux Unicode version    */
    const char *pieces[] = {                      /* Unicode chess pieces     */
      " ", "\u265a", "\u265f", "\u265e",          /* Black king, pawn, knight */
      "\u265d", "\u265c", "\u265b", " ",          /* Bishop, rook, queen      */
      " ", "\u2654", "\u2659", "\u2658",          /* White king, pawn, knight */
      "\u2657", "\u2656", "\u2655"                /* Bishop, rook, queen      */
    }; printf("\n   \u2554");                     /* Print top-left corner    */
    for (int col = 0; col < 8; col++)             /* Loop over board cols     */
      printf("\u2550\u2550\u2550\u2566");         /* Print top edge           */
    printf("\b\u2557\n");                         /* Print top-right corner   */
    for (int row = 0; row < 8; row++) {           /* Loop over board rows     */
    printf(" %d \u2551", flip ? row+1 : 8 - row); /* Print row and left edge  */
      for (int col = 0; col < 8; col++) {         /* Loop over board cols     */
        int file, rank;                           /* Flip file & rank holders */
        if (flip) {                               /* Black's perspective view */
          file = 8 - 1 - col;                     /* Mirror file              */
          rank = 8 - 1 - row;                     /* Mirror file              */
        } else { file = col; rank = row; }        /* White's perspective view */
        int sq = rank * 16 + file;                /* Calculate square index   */
        if (sq & 0x88) continue;                  /* Skip offboard squares    */
        int piece = board[sq] & 15;               /* Get piece code           */
        int light = (row + col) % 2 == 0;         /* Determine square color   */
        if (piece) {                              /* If occupied square       */
          printf(" %s ", pieces[piece]);          /* Print centered piece     */
        } else {                                  /* Empty square             */
          if (light) printf("   ");               /* Print light square       */
          else printf(" . ");                     /* Print dark square        */
        } printf("\033[0m\u2551");                /* Reset colors and border  */
      } printf("\n");                             /* Print new line           */
      if (row < 7) {                              /* Check if not last row    */
        printf("   \u2560");                      /* Print left intersection  */
        for (int col = 0; col < 8; col++)         /* Loop over board cols     */
          printf("\u2550\u2550\u2550\u256C");     /* Print horizontal divider */
        printf("\b\u2563\n");                     /* Print right intersection */
      }                                           /*                          */
    } printf("   \u255A");                        /* Print bottom-left corner */
    for (int col = 0; col < 8; col++)             /* Loop over board cols     */
      printf("\u2550\u2550\u2550\u2569");         /* Print bottom edge        */
    printf("\b\u255D\n\n");                       /* Print corner             */
    if (flip == 0) printf(                        /* Print files              */
      "     a   b   c   d   e   f   g   h\n\n"    /* White's perspective view */
    ); else printf(                               /*                          */
      "     h   g   f   e   d   c   b   a\n\n");  /* Black's perspective view */
  #endif                                          /* End platform check       */
}                                                 /* End printBoard function  */
/******************************************************************************/
char *moveStr(Move move) {                        /* Convert move to string   */
  static char mStr[6];                            /* Move string              */
  mStr[0] = (move.src & 7) + 'a',                 /* Source file              */
  mStr[1] = 8 - ((move.src >> 4) & 7) + '0',      /* Source rank              */
  mStr[2] = (move.dst & 7) + 'a',                 /* Target file              */
  mStr[3] = 8 - ((move.dst >> 4) & 7) + '0',      /* Target rank              */
  mStr[4] = "\0--nbrq"[move.prom];                /* Promoted piece           */
  return mStr;                                    /* Return move string       */
}                                                 /*                          */
/******************************************************************************/
char *gameStr() {                                 /* Game moves sequence      */
  static char gStr[2048];                         /* Game moves string        */
  int len = 0;                                    /* Offset length            */
  gStr[0] = '\0';                                 /* Reset moves string       */
  for (int i = 0; i < gamePly; i++) {             /* Loop over game moves     */
    len += sprintf(gStr + len, "%s ",             /* Update move offset       */
           moveStr(game[i].move));                /* Read next move           */
  } return gStr;                                  /* Return game string       */
}                                                 /*                          */
/******************************************************************************/
void printResult() {                              /* Game result              */
  MList mList; genMoves(&mList, 0);               /* Generate moves           */
  int legalMoves = 0;                             /* Legal moves counter      */
  for (int i = 0; i < mList.count; i++) {         /* Loop over move list      */
    if (make(mList.moves[i]) == 0) continue;      /* Make only legal moves    */
    legalMoves++; undo();                         /* Count moves, take back   */
  } if (legalMoves == 0) {                        /* No legal moves to make   */
    if (inCheck(side)) {                          /* Side to move in check    */
      if (side == 8) {                            /* White in check           */
        printf("0-1 { Black mates }\n");          /* Black wins               */
      } else printf("1-0 { White mates }\n");     /* Otherwise white wins     */
    } else printf("1/2-1/2 { Stalemate }\n");     /* Draw by stalemate        */
  } else if (reps() == 2) {                       /* Three fold repetition    */
    printf("1/2-1/2 { Draw by repetition }\n");   /* Draw by repetition       */
  } else if (fifty >= 100)                        /* Fifty move rule          */
 printf("1/2-1/2 { Draw by fifty move rule }\n"); /* Draw by fifty move rule  */
}                                                 /*                          */
/******************************************************************************/
Move parseMove(char *moveStr) {                   /* Parse move string        */
  Move illegal = { 0 };                           /* No move                  */
  int srcFile = moveStr[0] - 'a';                 /* Extract source file      */
  int srcRank = 8 - (moveStr[1] - '0');           /* Extract source rank      */
  int dstFile = moveStr[2] - 'a';                 /* Extract target file      */
  int dstRank = 8 - (moveStr[3] - '0');           /* Extract target rank      */
  int src = srcRank * 16 + srcFile;               /* Calculate source square  */
  int dst = dstRank * 16 + dstFile;               /* Calculate target square  */
  int prom = moveStr[4] & 0x5f;                   /* Extract promoted piece   */
  if (prom) {                                     /* Promoted piece available */
    switch (prom) {                               /* Process promoted piece   */
      case 'Q': prom = 6; break;                  /* Promoted piece is Queen  */
      case 'R': prom = 5; break;                  /* Promoted piece is Rook   */
      case 'B': prom = 4; break;                  /* Promoted piece is Bishop */
      case 'N': prom = 3; break;                  /* Promoted piece is Knight */
      default: prom = 0;                          /* No promotion is default  */
    }                                             /*                          */
  } MList mList; genMoves(&mList, 0);             /* Generate moves           */
  for (int i = 0; i < mList.count; i++) {         /* Loop over moves          */
    Move move = mList.moves[i];                   /* Get current move         */
    if (src == move.src &&                        /* User move matches        */
        dst == move.dst &&                        /* generated move           */
        prom == move.prom) {                      /* from the move list       */
      if (make(move)) {                           /* Legallity check          */
        undo();                                   /* Undo legal move          */
        return move;                              /* Return legal move        */
      }                                           /*                          */
    }                                             /*                          */
  } return illegal;                               /* Return illegal move      */
}                                                 /*                          */
/******************************************************************************/
void loadGame(char *moves) {                      /* Load user game           */
  char *mStr = strtok(moves, " ");                /* Extract game moves       */
  while (mStr != NULL) {                          /* Loop over game moves     */
    Move move = parseMove(mStr);                  /* Parse move               */
    if (move.pce == 0) {                          /* Illegal move             */
      printf("Illegal move: %s\n", mStr);         /* Inform user              */
      break;                                      /* Load no more moves       */
    } make(move);                                 /* Load game move           */
    mStr = strtok(NULL, " ");                     /* Extract next move        */
  } printBoard();                                 /* Print board to console   */
  printf("Game has been loaded\n");               /* Inform user game loaded  */
}                                                 /*                          */
/******************************************************************************/
/*                               CECP Protocol                                */
/*                                                                            */
/******************************************************************************/
int main() {                                      /* Xboard / Winboard driver */
  signal(SIGINT, SIG_IGN);                        /* Ignore INTERRUPT signal  */
  openBook();                                     /* Load opening book        */
  printf("MacroMix, v1.4 by CMK\n");              /* Engine info for GUI      */
  printf("Type \"help\" if stuck\n");             /* Help message             */
  char input[80] = "new";                         /* Run "new" to init engine */
  setbuf(stdout, NULL);                           /* Clear output buffer      */
  do {                                            /* User command loop        */
    char command[2048] = { 0 };                   /* User command storage     */                 
    sscanf(input, "%2047[^\n]", command);         /* Read user command        */
    if (!strcmp(command, "xboard")) {             /* CECP command "xboard"    */
      cli = 0; post = 1; continue;                /* Print PV by default      */
    } if (!strcmp(command, "cli")) {              /* CLI play mode            */
      cli = 1; printf("CLI mode\n");              /* Enter CLI mode           */
      post = 0;                                   /* Do not print PV line     */
      printBoard(); continue;                     /* Print board to console   */
    } if (!strcmp(command, "mode")) {             /* CLI command "mode"       */
      printf("%s mode\n", cli ?  "CLI":"Xboard"); /* Print current mode       */
      continue;                                   /* Process next command     */
    } if (!strncmp(command, "protover", 8)) {     /* CECP command "protover"  */
      printf("feature done=1\n");                 /* Prevent GUI delay        */
    } if (!strcmp(command, "new")) {              /* CECP command "new"       */
      srand(0);                                   /* Set random seed          */
      for (int col = 0; col < 2; col++)           /* Loop over color types    */
        for (int t = 0; t < 7; t++)               /* Loop over piece types    */
          for (int sq = 0; sq < 120; sq++)        /* Loop over board squares  */
            hashPiece[col][t][sq] = rand();       /* Init random hash number  */
      side = 8; ep = -1; castle = 15; fifty = 0;  /* Init position state      */
      kingSq[0] = 116; kingSq[1] = 4;             /* Init king squares        */
      for (int sq = 0; sq < 128; sq++) {          /* Loop over board squares  */
        board[sq] = 15;                           /* Castling bits (KQkq)     */
      } int white[8] = {                          /* White pieces             */
        13, 11, 12, 14, 9, 12, 11, 13             /* R, N, B, Q, K, B, N, R   */
      }; int i = 8; while (i--) {                 /* Loop over board files    */
        board[i+112] = white[i];                  /* Set up white pieces      */
        board[i+96] = 10;                         /* Set up white pawns       */
        board[i+32] = 0; board[i+48] = 0;         /* Set up empty squares     */
        board[i+64] = 0; board[i+80] = 0;         /* Set up empty squares     */
        board[i+16] = 18;                         /* Set up black pawns       */
        board[i] = board[i + 112] + 8;            /* Set up black pieces      */
      } board[8] = 7;                             /* Black O-O-O castling     */
        board[12] = 3;                            /* Black both castling      */
        board[15] = 11;                           /* Black O-O castling       */
        board[120] = 13;                          /* White O-O-O castling     */
        board[124] = 12;                          /* White both castling      */
        board[127] = 14;                          /* White O-O castling       */
        gamePly = 0;                              /* Reset game ply           */
        force = 0;                                /* Reset force mode         */
        setHash();                                /* Hash position            */
        if (cli) printBoard();                    /* Print board to console   */
        continue;                                 /* Process next command     */
    } if (!strncmp(command, "level", 5)) {        /* CECP command "level"     */
      float sec = 0; mps = 0;                     /* Reset level parameters   */
      sscanf(command, "level %d %*s %f",          /* Parse moves per session  */
             &mps, &sec);                         /* and time increment       */
      inc = sec * 1000; continue;                 /* Increment in ms          */
    } if (!strncmp(command, "time", 4)) {         /* CECP command "time"      */
      int ms = atoi(command + 5) * 10;            /* Remaining time in ms     */
      int mtg = 30;                               /* Default moves to go      */
      if (mps)                                    /* Moves per session given  */
        mtg = mps - (gamePly + 1) / 2 % mps;      /* Moves to time control    */
      if (mtg > 30) mtg = 30;                     /* Limit moves to go        */
      st = ms / mtg + inc;                        /* Set time control         */
      if (st > ms / 2) st = ms / 2;               /* Keep safety time margin  */
      sd = 63; continue;                          /* "Infinite" search depth  */
    } if (!strcmp(command, "white")) {            /* CECP command "white"     */
      force = 0; side = 8; continue;              /* Computer plays white     */
    } if (!strcmp(command, "black")) {            /* CECP command "black"     */
      force = 0; side = 16; continue;             /* Computer plays black     */
    } if (!strcmp(command, "force")) {            /* CECP command "force"     */
      force = 1; continue;                        /* Computer plays neither   */
    } if (!strncmp(command, "st", 2)) {           /* CECP command "st"        */
      st = atoi(command + 3) * 1000;              /* Fixed time search mode   */
      sd = 63; continue;                          /* "Infinite" search depth  */
    } if (!strncmp(command, "sd", 2)) {           /* CECP command "sd"        */
      sd = atoi(command + 3);                     /* Fixed depth search mode  */
      st = 1e6; continue;                         /* "Infinite" search time   */
    } if (!strcmp(command, "post")) {             /* CECP command "post"      */
      post = 1; continue;                         /* Set print PV flag        */
    } if (!strcmp(command, "nopost")) {           /* CECP command "nopost"    */
      post = 0; continue;                         /* Unset print PV flag      */
    } if (!strcmp(command, "go")) {               /* Make engine move         */
      force = 0;                                  /* Leave force mode         */
      think(); continue;                          /* Make engine move         */             
    } if (command[0] >= 'a' &&                    /*                          */
          command[0] <= 'z' &&                    /* If command is a move     */
          command[1] >= '0' &&                    /* in format "e2e4"         */
          command[1] <= '9') {                    /*                          */
      Move move = parseMove(command);             /* Parse user move          */
      if (move.pce == 0) {                        /* Not a legal user move    */
        printf("Illegal move: %s\n", command);    /* Report it to the GUI     */
        continue;                                 /* Do not search            */
      } else { setHash(); make(move); }           /* Make user move           */
      if (cli) { printResult(); printBoard(); }   /* Print board in CLI mode  */
      if (force == 0) {                           /* Search best move         */
        think();                                  /* No legal move to play    */
      } continue;                                 /* Read next command        */
    } if (!strcmp(command, "undo")) {             /* CECP command "undo"      */
      if (gamePly > 0) undo();                    /* Undo move if any         */
      if (cli) printBoard();                      /* Print board in CLI mode  */
      continue;                                   /* Process next command     */
    } if (!strncmp(command, "perft", 5)) {        /* Perft test               */
      tick = getTimeMs();                         /* Start timer              */
      nodes = 0;                                  /* Reset node count         */
      int d = atoi(command + 6);                  /* Parse perft depth        */
      printf("Perft depth %d ", d); perft(d);     /* Run test                 */
      int time = getTimeMs() - tick;              /* Stop timer               */
      printf("nodes %d time %d\n", nodes, time);  /* Print result             */
      continue;                                   /* Read next command        */
    } if (!strcmp(command, "board")) {            /* CLI command "board"      */
      printBoard(); continue;                     /* Print board to console   */
    } if (!strcmp(command, "flip")) {             /* CLI command "flip"       */
      flip ^= 1; printBoard(); continue;          /* Flip board view          */
    } if (!strcmp(command, "game")) {             /* CLI command "game"       */
      printf("%s\n", gameStr()); continue;        /* Print game moves         */
    } if (!strncmp(command, "load", 4)) {         /* CLI command "load"       */
      loadGame(command + 5); continue;            /* Load user game           */
    } if (!strcmp(command, "help")) {             /* CLI command "help"       */
      printf("\nxboard\tXboard mode\n");          /*                          */
      printf("cli\tCLI mode\n");                  /*                          */
      printf("mode\tcurrent mode\n");             /*                          */
      printf("new\tstart new game\n");            /*                          */
      printf("white\twhite to play\n");           /*                          */
      printf("black\tblack to play\n");           /*                          */
      printf("force\tedit mode\n");               /*                          */
      printf("st 5000\tfixed time\n");            /*                          */
      printf("sd 8\tfixed depth\n");              /*                          */
      printf("post\tprint PV lines\n");           /* Command reference list   */
      printf("nopost\tthink quietly\n");          /*                          */
      printf("go\tengine move\n");                /*                          */
      printf("e2e4\tuser move\n");                /*                          */
      printf("undo\tundo move\n");                /*                          */
      printf("perft 5\tperft depth 5\n");         /*                          */
      printf("board\tprint board\n");             /*                          */
      printf("flip\tflip board\n");               /*                          */
      printf("game\tprint moves\n");              /*                          */
      printf("load\tload e2e4 e7e5\n");           /*                          */
      printf("help\tthis reference\n");           /*                          */
      printf("quit\texit program\n\n");           /*                          */
      continue;                                   /* Process next command     */
    } if (!strcmp(command, "quit")) break;        /* CECP command "quit"      */
  } while (fgets(input, sizeof(input), stdin));   /* Read user input          */
  closeBook(); return 0;                          /* Close opening book, exit */
}                                                 /*                          */
/******************************************************************************/