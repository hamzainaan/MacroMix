/******************************************************************************/
/*                               MacroMix, v1.5                               */
/*                                                                            */
/*       A mix of most essential and basic chess programming techniques       */
/*          bundled into a single didactic CECP chess engine, by CMK          */
/*                                                                            */
/******************************************************************************/
/*                                  Features                                  */
/*                                                                            */
/******************************************************************************/
/* - Compact move generator                                                   */
/* - TT, PV, MVV/LVA, killer & history move ordering                          */
/* - Iterative deepening                                                      */
/* - Negamax with alpha-beta pruning                                          */
/* - Principal variation search                                               */
/* - Null move pruning                                                        */
/* - Late move reductions                                                     */
/* - Reverse futility pruning                                                 */
/* - Check extension                                                          */
/* - Transposition table (~1MB) with Zobrist hashing                          */
/* - Repetition & fifty move rule detection                                   */
/* - Quiescence search with delta pruning                                     */
/* - Simplified evaluation function                                           */
/* - Pawn structure: doubled, isolated & passed pawns                         */
/* - Texel tuned material, piece-square tables & pawn structure               */
/* - Triangular PV                                                            */
/* - Opening book with 240 lines up to 16 plies                               */
/* - CLI mode with board flip & game loading                                  */
/* - Winboard/Xboard (CECP) support                                           */
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
#include <math.h>
#ifdef WIN64                                      /*                          */
  #include "windows.h"                            /* WINDOWS GetTickCount()   */
  #include "conio.h"                              /* Used for inputWaiting()  */
#else                                             /*                          */
  #include "sys/time.h"                           /* UNIX-like gettimeofday() */
  #include "sys/select.h"                         /*                          */
#endif                                            /*                          */
/******************************************************************************/
/*                                Definitions                                 */
/*                                                                            */
/******************************************************************************/
#define VERSION "1.5"                             /* Engine version           */
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
long long nodes;                                  /* Number of positions      */ 
int board[128], side, ep, castle, fifty, hash;    /* Board position & state   */
int hashPiece[2][7][120];                         /* Random hash numbers      */
int kingSq[2];                                    /* King squares by color    */
int pieceScore, sideMaterial[2];                  /* Incremental eval sums    */
int pawnBits[2][10];                              /* Pawn rank bits by file   */
int lines[240];                                   /* Deltas on queen lines    */
int noCheck;                                      /* Hint: side not in check  */
int fromFen;                                      /* Position set up from FEN */
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
Move bestMove;                                    /* Best move to play        */
#define TT_SIZE (1 << 18)                         /* Transposition table size */
TTEntry tt[TT_SIZE];                              /* ~1mb transposition table */
/******************************************************************************/
int force, tick, stop, sd = 63, st = 1000, fixed; /* CECP control variables   */
int flip, cli = 1, post = 0, mps, inc, lag = 50;  /*                          */
int analyze;                                      /*                          */
/******************************************************************************/
int material[] = { /* Piece weights */ 0, 0, 100, 358, 397, 631, 1217 };    /**/
int pst[7][128] = {{ 0 },                                                   /**/
  {    /* Kings in the opening */          /* Kings in the endgame */       /**/
    -177, 90, 48, -5,-103,-93, 23, 50,    -78,-56,-36,-35,-25,  6,-15,-21,  /**/
     64, 52, 32, 42, 36, 78,  8, -7,    -13, 11,  0,  8,  6, 20, 10, -2,    /**/
     28, 55, 65, 42, 32, 70, 83, 16,      1, 12, 10,  4,  4, 32, 32, -2,    /**/
     14, 37, 30, 30, 14, 25, 19,-27,    -23,  9, 13, 13, 13, 18, 15, -9,    /**/
    -24,  8, 25,  8, 19,  5, -4,-30,    -28,-13,  8, 10, 10,  9, -3,-27,    /**/
    -10, 13, 16, 14, 15, 14, 11,-13,    -27,-16, -1,  4,  6,  0, -4,-22,    /**/
     -7, 17, 10,-16, -3,  5, 12,  0,    -41,-23, -8, -5, -4, -9,-16,-32,    /**/
    -40, 12,  5,-41, -8,-26,  8,-13,    -64,-39,-28,-27,-19,-24,-35,-53     /**/
  },                                                                        /**/
  {    /* Pawns */                                                          /**/
      0,  0,  0,  0,  0,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     81, 80, 44, 30, 29, 39, 45, 74,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     46, 47, 27,  2,  7, 27, 39, 33,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     16, 13,  4,  3,  9,  6, 11,  5,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0,  2, -1,  7,  7, -2,  1, -8,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -3, -3, -1, -9,  2,  1,  4, -7,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -6, -5,-12,-26,-13, 13,  9,-11,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0,  0,  0,  0,  0,  0,  0,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {    /* Knights */                                                        /**/
    -124,-52,-13,-29, -4,-49,-54,-124,      0,  0,  0,  0,  0,  0,  0,  0,  /**/
    -41, -9, 22, 23, 14, 13, -6,-38,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -23, 25, 36, 48, 45, 45, 33, -7,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -3, 24, 39, 56, 49, 54, 29, 11,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -10, 10, 33, 33, 39, 34, 28, -1,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -19,  3, 22, 30, 31, 29, 19,-13,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -25,-23, -2,  8,  9, 15,-12,-18,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -58,-19,-29,-13,-11,-16,-17,-47,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {    /* Bishops */                                                        /**/
    -37,-18,-34,-15,-18,-22,-11,-30,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -23, -1, -4,-16, 11, 12,  3,-38,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -11,  7, 15, 15, 13, 19, 18,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -2,  6, 19, 29, 30, 22,  6, -1,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -7,  9, 19, 28, 23, 13,  5, -2,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -6,  9, 16, 20, 17, 18,  9, -4,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     -7,  6,  8, -1, 10,  5, 21,-11,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -30, -6,-19,-12,-14,-16,-16,-27,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {    /* Rooks */                                                          /**/
     21, 18, 22, 27, 28, 13, 13, 12,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
     19, 23, 28, 29, 20, 22, 12, 14,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      7, 14, 12, 16,  5,  5,  8, -1,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
      0, -1, 12,  9,  9, 10, -6, -3,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -11, -3,  1,  1, -5,-10, -8,-17,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -25,-10,-11,-11, -8,-14,-14,-31,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -32,-12,-12, -8,-11, -8,-18,-52,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -20, -8,  0,  4,  3, -4,-28,-35,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
  },                                                                        /**/
  {     /* Queens */                                                        /**/
    -33,  6, 13, 16, 37, 23, 17, 33,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -45,-36,  1, 19, 23, 40, 32, 38,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -32,-20, -4, 24, 41, 52, 40, 43,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -27,-21, -5,  3, 22, 27, 19,  8,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -17,-14, -4, -2, 10,  7, 15,  0,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -21, -6, -4, -3, -3,  6,  5,  1,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -38,-19,  5, -4,  3, -2,-31,-21,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
    -15,-28,-19,  4,-17,-39,-41,-62,      0,  0,  0,  0,  0,  0,  0,  0,    /**/
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
void setSquare(int sq, int pce);                  /* Put piece on a square    */
void updateEval(int pce, int sq, int sign);       /* Add or remove its score  */
void initEval();                                  /* Score board from scratch */
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
void scoreMoves(MList *mList, int ply,            /* Score moves in order     */
                Move ttMove);                     /*                          */
int reps();                                       /* Find move repetitions    */
int inCheck(int color);                           /* Side to move under check */
void setHash();                                   /* Encode board position    */
int stateKey();                                   /* Castling, en passant key */
void ttStore(int key, int depth, int score,       /* Store move to            */
             int flag, int ply, Move move);       /* transposition table      */
int ttProbe(int key, int depth, int alpha,        /* Retrieve move from       */
            int beta, int *score, int ply,        /* transposition table      */
            Move *move);                          /*                          */
int getTimeMs();                                  /* Get time in ms           */
int inputWaiting();                               /* Interrupt search         */
int timeUp();                                     /* No more search time      */
void printBoard();                                /* Print board to           */
char *moveStr(Move move);                         /* Get move string          */
char *gameStr();                                  /* Get game moves string    */
void printResult();                               /* Print game result        */
void printHelp();                                 /* Print command reference  */
Move parseMove(char *moveStr);                    /* Parse move string        */
void loadGame(char *moves);                       /* Load game from string    */
int readInput(char *input, int size);             /* Read user input          */
void setBoard(char *fen);                         /* Set up board position    */
void newGame();                                   /* Start new game           */
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
          if (caps) break;                        /* No castling in captures  */
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
void setSquare(int sq, int pce) {                 /* Put piece on a square    */
  int old = board[sq];                            /* Piece leaving the square */
  if (old) {                                      /* Square was not empty     */
    hash ^= hashPiece[old >> 4][old & 7][sq];     /* Unhash leaving piece     */
    updateEval(old, sq, -1);                      /* Remove its eval score    */
  } board[sq] = pce;                              /* Set new square contents  */
  if (pce) {                                      /* New piece on the square  */
    hash ^= hashPiece[pce >> 4][pce & 7][sq];     /* Hash arriving piece      */
    updateEval(pce, sq, 1);                       /* Add its eval score       */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
int make(Move move) {                             /* Make move on board       */
  int king = kingSq[side >> 4];                   /* Own king square          */
  int safe = noCheck &&                           /* Not in check before move */
             (move.pce & 7) != 1 &&               /* Piece is not a king      */
             move.capSq == move.dst &&            /* No en passant capture    */
             !lines[move.src - king + 119];       /* Not on a line with king  */
  noCheck = 0;                                    /* Use the hint only once   */
  if (gamePly >= 1024) return 0;                  /* Game history is full     */
  game[gamePly].move = move;                      /* Save move into a game    */
  game[gamePly].side = side;                      /* Save side to move        */
  game[gamePly].ep = ep;                          /* Save en passant square   */
  game[gamePly].castle = castle;                  /* Save castling rights     */
  game[gamePly].fifty = fifty;                    /* Save fifty move count    */
  game[gamePly].hash = hash;                      /* Save hash key            */
  gamePly++;                                      /* Increment game ply       */
  hash ^= stateKey();                             /* Unhash castling & ep     */
  setSquare(move.capSq, 0);                       /* Empty captured square    */
  setSquare(move.src, 0);                         /* Empty source square      */
  setSquare(move.dst, move.pce);                  /* Move piece               */
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
      setSquare(move.dst, move.prom | side);      /* Promote to given piece   */
    }                                             /*                          */
  } if ((move.pce & 7) == 1 &&                    /* Piece is a king          */
     ((move.src == 116) || (move.src == 4))) {    /* Castling move            */
    switch(move.dst) {                            /* Where king castled       */
      case 118:                                   /* Square G1                */
        setSquare(117, 13); setSquare(119, 0);    /* Move rook from H1 to F1  */
        break;                                    /*                          */
      case 114:                                   /* Square C1                */
        setSquare(115, 13); setSquare(112, 0);    /* Move rook from A1 to D1  */
        break;                                    /*                          */
      case 6:                                     /* Square G8                */
        setSquare(5, 21); setSquare(7, 0);        /* Move rook from H8 to F8  */
        break;                                    /*                          */
      case 2:                                     /* Square C8                */
        setSquare(3, 21); setSquare(0, 0);        /* Move rook from A8 to D8  */
        break;                                    /*                          */
    }                                             /*                          */
  } castle &= board[move.src + 8];                /* Update castling source   */
  castle &= board[move.dst + 8];                  /* Update castling target   */
  if (move.capPce || (move.pce & 7) == 2)         /* On capture or pawn move  */
    fifty = 0;                                    /* Reset fifty count        */
  else fifty++;                                   /* Increment fifty move     */
  hash ^= hashPiece[side >> 4][0][0];             /* Unhash old side to move  */
  side = 24 - side;                               /* Swap the side to move    */
  hash ^= hashPiece[side >> 4][0][0];             /* Hash new side to move    */
  hash ^= stateKey();                             /* Hash new castling & ep   */
  if (!safe && inCheck(24 - side)) {              /* King in check after move */
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
  setSquare(move.dst, 0);                         /* Empty target square      */
  setSquare(move.src, move.pce);                  /* Move a piece back        */
  setSquare(move.capSq, move.capPce);             /* Restore captured piece   */
  if ((move.pce & 7) == 1)                        /* Piece is a king          */
    kingSq[side >> 4] = move.src;                 /* Restore king square      */
  if ((move.pce & 7) == 1 &&                      /* Piece is a king          */
     ((move.src == 116) || (move.src == 4))) {    /* Castling move            */
    switch(move.dst) {                            /* Where king castled       */
      case 118:                                   /* Square G1                */
        setSquare(119, 13); setSquare(117, 0);    /* Move rook from F1 to H1  */
        break;                                    /*                          */
      case 114:                                   /* Square C1                */
        setSquare(112, 13); setSquare(115, 0);    /* Move rook from D1 to A1  */
        break;                                    /*                          */
      case 6:                                     /* Square G8                */
        setSquare(7, 21); setSquare(5, 0);        /* Move rook from F8 to H8  */
        break;                                    /*                          */
      case 2:                                     /* Square C8                */
        setSquare(0, 21); setSquare(3, 0);        /* Move rook from D8 to A8  */
        break;                                    /*                          */
    }                                             /*                          */
  }                                               /*                          */
  hash = game[gamePly].hash;                      /* Load hash key            */
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
  if (bookFile != NULL && !fromFen) {             /* Skip if no book file     */
    fseek(bookFile, 0, SEEK_SET);                 /* Seek through book file   */
    while (fgets(bookLine, 2048, bookFile)) {     /* Read book line from file */
      int match = 1;                              /* Match line flag          */
      for (int i = 0; i<strlen(gameLine); i++) {  /* Compare game line to     */
        if (bookLine[i] == '\0' ||                /* each line                */
            bookLine[i] != gameLine[i]) {         /* in the opening book      */
          match = 0; break;                       /* End of book or mismatch  */
        }                                         /*                          */
      } if (match && bookMoves.count < 256) {     /* Game matches book line   */
        char *next = &bookLine[strlen(gameLine)]; /* Extract next book move   */
        Move move = parseMove(next);              /* Parse book move          */
        bookMoves.moves[bookMoves.count] = move;  /* Add book move to list    */
        if (move.pce) bookMoves.count++;          /* Count only legal moves   */
      }                                           /*                          */
    }                                             /*                          */
  } if (bookMoves.count == 0) return 0;           /* No book move found       */
  srand(time(NULL));                              /* Set random seed          */
  int rIndex = rand() % bookMoves.count;          /* Pick random move index   */
  Move bestMove = bookMoves.moves[rIndex];        /* Pick random book move    */
  setHash(); make(bestMove);                      /* Play book move on board  */
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
  tick = getTimeMs();                             /* Start timer              */
  if (!analyze && bookMove()) return;             /* Make book move if any    */
  stop = 0;                                       /* Reset stop flag          */
  nodes = 0;                                      /* Reset position count     */
  memset(pvTable,0,sizeof(pvTable));              /* Reset PV table           */
  memset(pvLength,0,sizeof(pvLength));            /* Reset PV length table    */
  memset(killMoves,0,sizeof(killMoves));          /* Reset killers table      */
  memset(histScores,0,sizeof(histScores));        /* Reset history table      */
  bestMove = (Move){ 0 };                         /* Init PV move holder      */
  for (int d = 1; d <= sd; d++) {                 /* Iterative deepening      */
    int s = search(d, 0, -50000, 50000, 1);       /* Search best move         */
    if (stop) break;                              /* Time is up or game over  */
    bestMove = pvTable[0][0];                     /* Record last best move    */
    if (post) { printf("%d %d %d %lld ",          /* Engine search output     */
      d, s, (getTimeMs()-tick)/10, nodes);        /* depth/score/time/nodes   */
    for (int i = 0; i < pvLength[0]; i++) {       /* Loop over PV line        */
      Move move = pvTable[0][i];                  /* Get PV move              */
      printf("%s", moveStr(move));                /* Print PV move            */
      if (i < pvLength[0] - 1)                    /* Not the last PV move     */
        printf(" ");                              /* Print separator          */
    } printf("\n");                               /* Print new line           */
    } if (abs(s) > 48000) break;                  /* Stop if found mate       */
  } if (bestMove.pce == 0) return;                /* No legal move to play    */
  if (analyze) return;                            /* Analysis mode            */
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
  Move ttMove = { 0 }, best = { 0 };              /* TT move & best move      */
  if (depth == 0 && !inCheck(side))               /* Main search is done      */
    return quiesce(ply, alpha, beta);             /* Do quiescence search     */
  nodes++;                                        /* Count searched positions */
  if ((nodes & 1023) == 0 && bestMove.pce)        /* Check time control       */
    if (timeUp()) stop = 1;                       /* Time is up, set stop     */
  pvLength[ply] = ply;                            /* Init PV length           */
  if (ply && reps()) return 0;                    /* 3 Fold repetition, draw  */
  int pvNode = (beta - alpha) > 1;                /* Distinguish PV nodes     */
  int key  = hash ^ (side == 8 ?                  /* Get position key         */
             0xAAAAAAAAu : 0x55555555u);          /* relative to side to move */                                
  int ttHit = ttProbe(key, depth, alpha, beta,    /* Read TT entry            */
                      &score, ply, &ttMove);      /*                          */
  if (ply && ttHit && pvNode == 0) return score;  /* Return TT score          */
  if (ply >= 62) return eval();                   /* Too deep, evaluate       */
  if (gamePly >= 1022) return eval();             /* Too deep, evaluate       */
  int check = inCheck(side);                      /* King in check flag       */
  if (check) depth++;                             /* King in check, go deeper */  
  int staticEval = check ? -50000 : eval();
  if (pvNode == 0 && check == 0 && ply &&         /* Reverse futility pruning */
      depth <= 3 && abs(beta) < 48000) {          /* at shallow non-PV nodes  */
    if (staticEval - 120 * depth >= beta)         /* Static score far above   */
      return beta;                                /* beta, prune the node     */
  }                                               /*                          */
  if (check == 0 && ply && null &&                /* Null move pruning        */
      staticEval >= beta) {
    int matScore = 0;                             /* Material score           */
    for (int sq = 0; sq < 120; sq++) {            /* Loop over board squares  */
      if (sq & 0x88) { sq += 7; continue; }       /* Skip offboard square     */
      int pce = board[sq], pceType = pce & 7;     /* Get piece. extract type  */
      if (pce && pceType > 2 && (pce & side)) {   /* Pieces of side to move   */
        matScore += material[pceType];            /* Count all but pawns      */
      }                                           /*                          */
    } int rDepth = 3 + depth / 6;                 /* Calculat reduce depth    */
    if (depth > rDepth && matScore > 1300) {      /* Null move threshold      */
      int oldSide = side, oldEp = ep;             /*                          */
      int oldCastle = castle, oldHash = hash;     /* Preserve position state  */
      int oldFifty = fifty;                       /*                          */
      hash ^= stateKey(); ep = -1;                /* No en passant after null */
      hash ^= hashPiece[side >> 4][0][0];         /* Unhash old side to move  */
      side = 24 - side;                           /* Swap the side            */
      hash ^= hashPiece[side >> 4][0][0];         /* Hash new side to move    */
      hash ^= stateKey();                         /* Hash castling, no ep     */
      score = -search(depth - 1 - rDepth,         /* Use reduced depth        */
                   ply+1,-beta,-beta+1, 0);       /* Negamax search           */
      side = oldSide, ep = oldEp;                 /*                          */
      castle = oldCastle, hash = oldHash;         /* Restore position state   */
      fifty = oldFifty;                           /*                          */
      if (stop) return 0;                         /* Time is up, return       */
      if (score >= beta) return beta;             /* Beta cutoff              */
    }                                             /*                          */
  } MList mList; genMoves(&mList, 0);             /* Generate moves           */
  scoreMoves(&mList, ply, ttMove);                /* Score moves              */
  for (int i = 0; i < mList.count; i++) {         /* Loop over move list      */  
    pickMove(&mList, i);                          /* Bring best move forward  */
    Move move = mList.moves[i];                   /* Get current move         */
    noCheck = !check;                             /* Hint for legality test   */
    if (make(move) == 0) continue;                /* Make move, skip illegals */
    if (legalMoves == 0)                          /* First move               */
      score = -search(depth-1, ply+1,             /* Full window search       */
                      -beta, -alpha, 1);          /*                          */
    else {                                        /* Other moves              */
        int quiet =                               /* Reduce late quiet moves  */
                check == 0 && move.prom == 0 &&   /* when not in check, not   */
                move.score < 800000000 &&         /* TT/PV, capture, killer,  */
                inCheck(side) == 0;               /* or a checking move       */
        if (quiet && !pvNode && depth <= 3 &&
            legalMoves >= 3 + depth * depth) {
          undo(); legalMoves++; continue;
        }
        int r = 0;
        if (quiet && depth >= 3 && legalMoves >= 2) {
          r = (int)(0.75 + log(depth) * log(legalMoves + 1) / 2.25);
          if (pvNode && r > 0) r--;
          if (r > depth - 2) r = depth - 2;
          if (r < 0) r = 0;
        }
        score = -search(depth-1-r, ply+1,         /* PVS null window search   */
                        -alpha-1, -alpha, 1);     /*                          */
        if (r && score > alpha)                   /* Reduced move fails high  */
            score = -search(depth-1, ply+1,       /* Re-search at full depth  */
                            -alpha-1, -alpha, 1); /*                          */
        if (score > alpha && score < beta)        /* Found a better move      */
            score = -search(depth-1, ply+1,       /* Full window re-search    */
                            -beta, -alpha, 1);    /*                          */
    } undo();                                     /* Take move back           */
    legalMoves++;                                 /* Count legal moves        */
    if (stop) return 0;                           /* Time is up, return       */
    if (score > alpha) {                          /* Found a better move      */
      best = move;                                /* Remember best move       */
      if (move.capPce == 0)                       /* Quiet move               */
        histScores[board[move.src] & 15]          /* Update history scores    */
          [move.dst] += depth;                    /* By depth count           */
      if (score >= beta) {                        /* Found a killer move      */
        if (move.capPce == 0) {                   /* Quiet move               */
          killMoves[1][ply] = killMoves[0][ply];  /* Update 2nd killer move   */
          killMoves[0][ply] = move;               /* Update 1st killer move   */
        } alpha = beta; break;                    /* Beta cutoff              */
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
  ttStore(key, depth, alpha, flag, ply, best);    /* Write TT entry           */
  return alpha;                                   /* Return best score        */
}                                                 /*                          */
/******************************************************************************/
int quiesce(int ply, int alpha, int beta) {       /* Quiescence search        */
  if (stop) return 0;                             /* Time is up, return       */
  nodes++;                                        /* Count searched positions */
  if ((nodes & 1023) == 0 && bestMove.pce)        /* Check time control       */
    if (timeUp()) stop = 1;                       /* Time is up, set stop     */
  pvLength[ply] = ply;                            /* Init PV length           */
  if (ply >= 62) return eval();                   /* Too deep, evaluate       */
  if (gamePly >= 1022) return eval();             /* Too deep, evaluate       */
  int score = eval();                             /* Evaluate position        */
  if (score >= beta) return beta;                 /* Beta cutoff              */
  if (score > alpha) alpha = score;               /* Update alpha score       */
  MList mList; genMoves(&mList, 1);               /* Generate moves           */
  scoreMoves(&mList, ply, (Move){ 0 });           /* Score moves              */
  for (int i = 0; i < mList.count; i++) {         /* Loop over move list      */  
    pickMove(&mList, i);                          /* Bring best move forward  */
    Move move = mList.moves[i];                   /* Get current move         */
    if (score + material[move.capPce & 7] +       /* Delta pruning, capture   */
        200 <= alpha && move.prom == 0)           /* can not raise alpha      */
      continue;                                   /* Skip hopeless capture    */
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
  int score = pieceScore;                         /* Material & positional    */
  int kingWhite = kingSq[0];                      /* White king square        */
  int kingBlack = kingSq[1];                      /* Black king square        */
  for (int col = 0; col < 2; col++)               /* Loop over both colors    */
    for (int file = 1; file < 9; file++)          /* Loop over padded files   */
      for (int rank = 1;                          /* Loop over pawn ranks     */
           rank < 7 && pawnBits[col][file];       /* on files that have pawns */
           rank++) {                              /*                          */
        if (!(pawnBits[col][file] >> rank & 1))   /* No pawn on the square    */
          continue;                               /* Skip to next square      */
        int pce = col ? 18 : 10;                  /* Pawn of this color       */
        int *own = pawnBits[col];                 /* Own pawns by file        */
        int *opp = pawnBits[col ^ 1];             /* Enemy pawns by file      */
        int ahead = col ? 255 << (rank + 1)       /* Ranks ahead of the pawn  */
                        : (1 << rank) - 1;        /* for black or for white   */
        int isolated = !own[file - 1] &&          /* No own pawns on          */
                       !own[file + 1];            /* adjacent files           */
        int doubled = own[file] & ahead;          /* Own pawn ahead on file   */
        int passed = !((opp[file - 1] | opp[file] /* No enemy pawns ahead on  */
                      | opp[file + 1]) & ahead);  /* same or adjacent files   */
        if (doubled) {                            /* Pawn is doubled          */
            if (pce == 10) score -= 9;            /* Give penalty to white    */
            else score += 9;                      /* Give penalty to black    */
        } if (isolated) {                         /* Pawn is isolated         */
            if (pce == 10) score -= 14;           /* Give penalty to white    */
            else score += 14;                     /* Give penalty to black    */
        } if (passed) {                           /* Pawn is passed           */
            int adv = (pce == 10) ?               /* For both colors          */
                6 - rank : rank - 1;              /* Calculate pawn rank      */
            int bonus = 1 + adv * adv * 6;        /* Passed pawn bonus        */
            if (pce == 10) score += bonus;        /* Give bonus to white      */
            else score -= bonus;                  /* Give bonus to black      */
        }                                         /*                          */
      }                                           /*                          */
  if (sideMaterial[0] < 1350) kingWhite += 8;     /* White king in an endgame */
  if (sideMaterial[1] < 1350) kingBlack += 8;     /* Black king in an endgame */
  score += pst[1][kingWhite];                     /* White king score         */
  score -= pst[1][kingBlack ^ 112];               /* Black king score         */
  return (side == 8) ? score : -score;            /* Side respective score    */
}                                                 /*                          */
/******************************************************************************/
void updateEval(int pce, int sq, int sign) {      /* Add or remove its score  */
  int col = pce >> 4, type = pce & 7;             /* Piece color & type       */
  sideMaterial[col] += sign * material[type];     /* Count side material      */
  if (type == 2)                                  /* Piece is a pawn          */
    pawnBits[col][(sq & 7) + 1] ^=                /* Toggle its rank bit      */
      1 << (sq >> 4);                             /* on its padded file       */
  if (type == 1) return;                          /* Kings are scored in eval */
  if (col) { sign = -sign; sq ^= 112; }           /* Black: negate & mirror   */
  pieceScore += sign * material[type];            /* Material score           */
  pieceScore += sign * pst[type][sq];             /* Positional score         */
}                                                 /*                          */
/******************************************************************************/
void initEval() {                                 /* Score board from scratch */
  pieceScore = 0;                                 /* Reset piece score        */
  sideMaterial[0] = sideMaterial[1] = 0;          /* Reset side material      */
  memset(pawnBits, 0, sizeof(pawnBits));          /* Reset pawn rank bits     */
  for (int sq = 0; sq < 120; sq++) {              /* Loop over board squares  */
    if (sq & 0x88) { sq += 7; continue; }         /* Skip offboard squares    */
    if (board[sq])                                /* Square has a piece       */
      updateEval(board[sq], sq, 1);               /* Add its eval score       */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
void pickMove(MList *mList, int mv1) {            /* Pick best remaining move */
  int count = mList->count, best = mv1;           /* Move list length         */
  for (int mv2 = mv1 + 1; mv2 < count; mv2++) {   /* Loop over next moves     */
    if (mList->moves[best].score <                /* Current score            */
        mList->moves[mv2].score) best = mv2;      /* Less than next score     */
    if (mv2 == count - 1 && best != mv1) {
      Move temp = mList->moves[mv1];              /* Create temp move         */
      mList->moves[mv1] = mList->moves[best];     /* Higher score goes first  */
      mList->moves[best] = temp;                  /* Lower score goes after   */
    }                                             /*                          */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
void scoreMoves(MList *mList, int ply,            /* Move scoring             */
                Move ttMove) {                    /*                          */
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
    if (move->src == ttMove.src &&                /* Move is a TT move        */
        move->dst == ttMove.dst &&                /*                          */
        move->prom == ttMove.prom)                /*                          */
      move->score = 2100000000;                   /* Search it first          */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
int reps() {                                      /* Repetition detection     */
  int reps = 0;                                   /* Position repetitions     */
  for (int i = gamePly - 2;                       /* Loop over game history   */
       i >= 0 && i >= gamePly - fifty; i -= 2)    /* Same side to move only   */
    if (game[i].hash == hash) reps++;             /* Found repetition         */
  return reps;                                    /* Times position repeats   */
}                                                 /*                          */
/******************************************************************************/
int inCheck(int color) {                          /* Is king in check ?       */
  return attack(kingSq[color >> 4], 24 - color);  /* King attacked by enemy?  */
}                                                 /*                          */
/******************************************************************************/
void setHash() {                                  /* Encode board position    */
  hash = hashPiece[side >> 4][0][0];              /* Side to move hash key    */
  hash ^= stateKey();                             /* Castling, en passant key */
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
int stateKey() {                                  /* Castling, en passant key */
  int key = hashPiece[1][0][castle + 1];          /* Castling rights key      */
  int pawn = 2 | side, sq = ep ^ 16;              /* Own pawn, pushed pawn sq */
  if (ep != -1 && (board[sq - 1] == pawn ||       /* Hash en passant square   */
      board[sq + 1] == pawn))                     /* only if a pawn of the    */
    key ^= hashPiece[0][0][ep];                   /* side to move can capture */
  return key;                                     /* Return state key         */
}                                                 /*                          */
/******************************************************************************/
void ttStore(int key, int depth, int score,       /* Write entry to TT        */
             int flag, int ply, Move move) {      /*                          */
    TTEntry *e = &tt[key & (TT_SIZE - 1)];        /* Create TT entry          */
    if (depth >= e->depth || e->key != key) {     /* Replace old entry        */
        if (move.pce || e->key != key)            /* Keep old move if no best */
            e->move = move;                       /* Write best move          */
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
            int beta, int *score, int ply,        /*                          */
            Move *move) {                         /*                          */
    TTEntry *e = &tt[key & (TT_SIZE - 1)];        /* Create TT entry          */
    if (e->key != key) return 0;                  /* Skip on key mismatch     */
    *move = e->move;                              /* Read TT move for sorting */
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
int inputWaiting() {                              /*                          */
  #ifdef WIN64                                    /*                          */
    DWORD bytes;                                  /*                          */
    HANDLE in = GetStdHandle(STD_INPUT_HANDLE);   /*                          */
    if (PeekNamedPipe(in, NULL, 0, NULL,          /* While engine is          */
        &bytes, NULL)) return bytes > 0;          /* searching listen to      */
    return _kbhit();                              /* keyboard hit, so if any  */
  #else                                           /* command is send to       */
    fd_set fds;                                   /* the engine it stops      */
    struct timeval t = { 0, 0 };                  /* searching                */
    FD_ZERO(&fds); FD_SET(0, &fds);               /*                          */
    return select(1, &fds, NULL, NULL, &t) > 0;   /*                          */
  #endif                                          /*                          */
}                                                 /*                          */
/******************************************************************************/
int timeUp() {                                    /*                          */
  if (!analyze) return getTimeMs() - tick >= st;  /*                          */
  if (!inputWaiting()) return 0;                  /*                          */
  int c = getc(stdin);                            /* Stop searching           */
  if (c != '.') { ungetc(c, stdin); return 1; }   /* when time is up          */
  while (c != '\n' && c != EOF) c = getc(stdin);  /*                          */
  return 0;                                       /*                          */
}                                                 /*                          */
/******************************************************************************/
/*                                     I/O                                    */
/*                                                                            */
/******************************************************************************/
void printBoard() {                               /* Print board to console   */  
  printf("\n");                                   /* Print new line           */
  for (int row = 0; row < 8; row++) {             /* Loop over board rows     */
    for (int col = 0; col < 16; col++) {          /* Loop over board cols     */
      if (col == 0)                               /* On board edge            */
      printf(" %d  ", flip ? row + 1:8 - row);    /* Print row number         */
      int file, rank;                             /* Flip file & rank holders */
      if (flip) {                                 /* Black's perspective view */
        file = 8 - 1 - col;                       /* Mirror file              */
        rank = 8 - 1 - row;                       /* Mirror rank              */
      } else { file = col; rank = row; }          /* White's perspective view */
      int sq = rank * 16 + file;                  /* Calculate square index   */
      if (sq & 0x88) continue;                    /* Skip offboard squares    */
      #ifdef WIN64                                /* Windows ASCII version    */
        char p[] = ".kpnbrq--KPNBRQ";             /* ASCII piece characters   */
        printf("%c ", p[board[sq] & 15]);         /* Print piece              */
      # else                                      /* Unicode piece characters */
        const char *pieces[] = {                  /* Unicode chess pieces     */
          ".", "\u265a", "\u265f", "\u265e",      /* Black king, pawn, knight */
          "\u265d", "\u265c", "\u265b", " ",      /* Bishop, rook, queen      */
          " ", "\u2654", "\u2659", "\u2658",      /* White king, pawn, knight */
          "\u2657", "\u2656", "\u2655"            /* Bishop, rook, queen      */
      }; printf("%s ", pieces[board[sq] & 15]);   /* Print piece              */
      #endif                                      /*                          */
    } printf("\n");                               /* Print new line           */
  } if (flip == 0)                                /* Print files              */
  printf("\n    a b c d e f g h\n\n");            /* White's perspective view */
  else printf("\n    h g f e d c b a\n\n");       /* Black's perspective view */
}                                                 /*                          */
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
  static char gStr[6145];                         /* Game moves string        */
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
  int legalMoves = 0; setHash();                  /* Legal moves counter      */
  for (int i = 0; i < mList.count; i++) {         /* Loop over move list      */
    if (make(mList.moves[i]) == 0) continue;      /* Make only legal moves    */
    legalMoves++; undo();                         /* Count moves, take back   */
  } if (legalMoves == 0) {                        /* No legal moves to make   */
    if (inCheck(side)) {                          /* Side to move in check    */
      if (side == 8) {                            /* White in check           */
        printf("0-1 { Black mates }\n");          /* Black wins               */
      } else printf("1-0 { White mates }\n");     /* Otherwise white wins     */
    } else printf("1/2-1/2 { Stalemate }\n");     /* Draw by stalemate        */
  } else if (reps() >= 2) {                       /* Three fold repetition    */
    printf("1/2-1/2 { Draw by repetition }\n");   /* Draw by repetition       */
  } else if (fifty >= 100)                        /* Fifty move rule          */
 printf("1/2-1/2 { Draw by fifty move rule }\n"); /* Draw by fifty move rule  */
}                                                 /*                          */
/******************************************************************************/
void printHelp() {                                /* Print command reference  */
  printf("\nxboard\tXboard mode\n");              /*                          */
  printf("cli\tCLI mode\n");                      /*                          */
  printf("mode\tcurrent mode\n");                 /*                          */
  printf("new\tstart new game\n");                /*                          */
  printf("white\twhite to play\n");               /*                          */
  printf("black\tblack to play\n");               /*                          */
  printf("force\tedit mode\n");                   /*                          */
  printf("st 5000\tfixed time\n");                /*                          */
  printf("sd 8\tfixed depth\n");                  /*                          */
  printf("post\tprint PV lines\n");               /*                          */
  printf("nopost\tthink quietly\n");              /*                          */
  printf("go\tengine move\n");                    /* Command reference list   */
  printf("analyze\tinfinite analysis\n");         /*                          */
  printf("exit\tstop analysis\n");                /*                          */
  printf("e2e4\tuser move\n");                    /*                          */
  printf("undo\tundo move\n");                    /*                          */
  printf("perft 5\tperft depth 5\n");             /*                          */
  printf("board\tprint board\n");                 /*                          */
  printf("flip\tflip board\n");                   /*                          */
  printf("game\tprint moves\n");                  /*                          */
  printf("load\tload e2e4 e7e5\n");               /*                          */
  printf("help\tthis reference\n");               /*                          */
  printf("quit\texit program\n\n");               /*                          */
}                                                 /*                          */
/******************************************************************************/
Move parseMove(char *moveStr) {                   /* Parse move string        */
  Move illegal = { 0 };                           /* No move                  */
  if (strlen(moveStr) < 4) return illegal;        /* Move string too short    */
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
    } setHash(); make(move);                      /* Load game move           */
    mStr = strtok(NULL, " ");                     /* Extract next move        */
  } printBoard();                                 /* Print board to console   */
  printf("Game has been loaded\n");               /* Inform user game loaded  */
}                                                 /*                          */
/******************************************************************************/
int readInput(char *input, int size) {            /* Read user input          */
  if (analyze) think();                           /* Make engine move         */
  return fgets(input, size, stdin) != NULL;       /* Read command from stdin  */
}                                                 /*                          */
/******************************************************************************/
/*                                    Init                                    */
/*                                                                            */
/******************************************************************************/
void setBoard(char *fen) {                        /* Parse FEN                */
  char pos[128] = "", stm = 'w';                  /* Position and side        */
  char eps[3] = "", cas[5] = "";                  /* En passant and castling  */
  char *pieces = "kpnbrqKPNBRQ";                  /* Chess pieces             */
  fifty = 0;                                      /* Fifty move rule          */
  sscanf(fen, "%127s %c %4s %2s %d",              /* Read FEN string          */
         pos, &stm, cas, eps, &fifty);            /*                          */
  for (int sq = 0; sq < 128; sq++)                /* Loop over board square   */
    if (!(sq & 0x88)) board[sq] = 0;              /* Clear board squares      */
  int sq = 0;                                     /* Reset square             */
  for (char *c = pos; *c && sq < 128; c++) {      /* Loop over position chars */
    char *p = strchr(pieces, *c);                 /* Get current char         */
    if (*c == '/') sq = (sq + 15) & ~15;          /* Parse end of file        */
    else if (*c >= '1' && *c <= '8')              /* Encountered a digit      */
      sq += *c - '0';                             /* Parse skipped files      */
    else if (p && !(sq & 0x88)) {                 /* Encountered a piece      */
      int color = p - pieces < 6 ? 16 : 8;        /* Get piece color          */
      int type = (p - pieces) % 6 + 1;            /* Get piece type           */
      board[sq] = color | type;                   /* Set piece on board       */
      if (type == 1) kingSq[color >> 4] = sq;     /* Init king square         */
      sq++;                                       /* Go to next square        */
    }                                             /*                          */
  } side = stm == 'b' ? 16 : 8;                   /* Parse side to move       */
  castle = (strchr(cas, 'K') ? 1 : 0) |           /* Parse castling rights    */
           (strchr(cas, 'Q') ? 2 : 0) |           /*                          */
           (strchr(cas, 'k') ? 4 : 0) |           /*                          */
           (strchr(cas, 'q') ? 8 : 0);            /*                          */
  ep = -1;                                        /* Reset en passant square  */
  if (eps[0] >= 'a' && eps[0] <= 'h' &&           /* Parse en passant square  */
      eps[1] >= '1' && eps[1] <= '8')             /*                          */
    ep = (eps[0] - 'a') +                         /*                          */
         (8 - (eps[1] - '0')) * 16;               /*                          */
  gamePly = 0;                                    /* Reset game ply           */
  fromFen = 1;                                    /* Book lines do not apply  */
  setHash();                                      /* Hash position            */
  initEval();                                     /* Score position           */
}                                                 /*                          */
/******************************************************************************/
void newGame() {                                  /* Start new game           */
  srand(0);                                       /* Set random seed          */
  for (int col = 0; col < 2; col++)               /* Loop over color types    */
    for (int t = 0; t < 7; t++)                   /* Loop over piece types    */
      for (int sq = 0; sq < 120; sq++)            /* Loop over board squares  */
        hashPiece[col][t][sq] = rand();           /* Init random hash number  */
  side = 8; ep = -1; castle = 15; fifty = 0;      /* Init position state      */
  kingSq[0] = 116; kingSq[1] = 4;                 /* Init king squares        */
  for (int sq = 0; sq < 128; sq++) {              /* Loop over board squares  */
    board[sq] = 15;                               /* Castling bits (KQkq)     */
  } int white[8] = {                              /* White pieces             */
    13, 11, 12, 14, 9, 12, 11, 13                 /* R, N, B, Q, K, B, N, R   */
  }; int i = 8; while (i--) {                     /* Loop over board files    */
    board[i+112] = white[i];                      /* Set up white pieces      */
    board[i+96] = 10;                             /* Set up white pawns       */
    board[i+32] = 0; board[i+48] = 0;             /* Set up empty squares     */
    board[i+64] = 0; board[i+80] = 0;             /* Set up empty squares     */
    board[i+16] = 18;                             /* Set up black pawns       */
    board[i] = board[i + 112] + 8;                /* Set up black pieces      */
  } board[8] = 7;                                 /* Black O-O-O castling     */
    board[12] = 3;                                /* Black both castling      */
    board[15] = 11;                               /* Black O-O castling       */
    board[120] = 13;                              /* White O-O-O castling     */
    board[124] = 12;                              /* White both castling      */
    board[127] = 14;                              /* White O-O castling       */
    gamePly = 0;                                  /* Reset game ply           */
    fromFen = 0;                                  /* Game from start position */
    force = 0;                                    /* Reset force mode         */
    setHash();                                    /* Hash position            */
    initEval();                                   /* Score position           */
    if (cli) printBoard();                        /* Print board to console   */
}                                                 /*                          */
/******************************************************************************/
/*                               CECP Protocol                                */
/*                                                                            */
/******************************************************************************/
int main() {                                      /* Xboard / Winboard driver */
  signal(SIGINT, SIG_IGN);                        /* Ignore INTERRUPT signal  */
  openBook();                                     /* Load opening book        */
  for (int d = 0; d < 8; d++)                     /* Loop over queen offsets  */
    for (int n = 1; n < 8; n++)                   /* Loop over ray distances  */
      lines[offsets[6][d] * n + 119] = 1;         /* Mark square delta        */
  printf("MacroMix, v%s by CMK\n", VERSION);      /* Engine info for GUI      */
  printf("Type \"help\" if stuck\n");             /* Help message             */
  char input[2048] = "new";                       /* Run "new" to init engine */
  setbuf(stdout, NULL);                           /* Clear output buffer      */
  setbuf(stdin, NULL);                            /* Clear input buffer       */
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
      printf("feature ping=1 setboard=1\n");      /* List engine features     */
      printf("feature done=1\n");                 /* Prevent GUI delay        */
    } if (!strncmp(command, "ping", 4)) {         /* CECP "ping" command      */
      printf("pong %s\n", command + 5); continue; /* Respond with "pong"      */
    } if (!strcmp(command, "new")) {              /* CECP command "new"       */
      newGame();                                  /* Start new game           */
      continue;                                   /* Process next command     */
    } if (!strncmp(command, "setboard", 8)) {     /* CECP "new" command       */
      setBoard(command + 8);                      /* Set board from FEN       */
      if (cli) printBoard();                      /* Print board in CLI mode  */
      continue;                                   /* Process next command     */
    } if (!strncmp(command, "level", 5)) {        /* CECP command "level"     */
      float sec = 0; mps = 0; fixed = 0;          /* Reset level parameters   */
      sscanf(command, "level %d %*s %f",          /* Parse moves per session  */
             &mps, &sec);                         /* and time increment       */
      inc = sec * 1000; continue;                 /* Increment in ms          */
    } if (!strncmp(command,"time",4) && !fixed) { /* CECP command "time"      */
      int ms = atoi(command + 5) * 10 - lag;      /* Remaining time in ms     */
      int mtg = 30;                               /* Default moves to go      */
      if (mps)                                    /* Moves per session given  */
        mtg = mps - gamePly / 2 % mps;            /* Moves to time control    */
      if (mtg > 30) mtg = 30;                     /* Limit moves to go        */
      st = ms / mtg + inc;                        /* Set time control         */
      if (st > ms / 2) st = ms / 2;               /* Keep safety time margin  */
      continue;                                   /* Process next command     */
    } if (!strcmp(command, "white")) {            /* CECP command "white"     */
      force = 0; side = 8; setHash(); continue;   /* Computer plays white     */
    } if (!strcmp(command, "black")) {            /* CECP command "black"     */
      force = 0; side = 16; setHash(); continue;  /* Computer plays black     */
    } if (!strcmp(command, "force")) {            /* CECP command "force"     */
      force = 1; continue;                        /* Computer plays neither   */
    } if (!strncmp(command, "st", 2)) {           /* CECP command "st"        */
      st = atoi(command + 3) * 1000 - lag;        /* Fixed time search mode   */
      sd = 63; fixed = 1; continue;               /* "Infinite" search depth  */
    } if (!strncmp(command, "sd", 2)) {           /* CECP command "sd"        */
      sd = atoi(command + 3);                     /* Fixed depth search mode  */
      st = 1e6; continue;                         /* "Infinite" search time   */
    } if (!strcmp(command, "post")) {             /* CECP command "post"      */
      post = 1; continue;                         /* Set print PV flag        */
    } if (!strcmp(command, "nopost")) {           /* CECP command "nopost"    */
      post = 0; continue;                         /* Unset print PV flag      */
    } if (!strcmp(command, "go")) {               /* Make engine move         */
      force = 0;                                  /* Leave force mode         */
      if (!analyze) think();                      /* Make engine move         */             
      continue;                                   /* Process next command     */
    } if (!strcmp(command, "analyze")) {          /* CECP "analyze" command   */
      analyze = 1; post = 1; continue;            /* Start infinite analysis  */
    } if (!strcmp(command, "exit")) {             /* CECP "exit" command      */
      analyze = 0; continue;                      /* Stop infinite analysis   */
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
      if (force == 0 && !analyze) {               /* Search best move         */
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
      printf("nodes %lld time %d\n",nodes,time);  /* Print result             */
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
      printHelp();                                /* Print command reference  */
      continue;                                   /* Process next command     */
    } if (!strcmp(command, "quit")) break;        /* CECP command "quit"      */
  } while (readInput(input, sizeof(input)));      /* Read user input          */
  closeBook(); return 0;                          /* Close opening book, exit */
}                                                 /*                          */
/******************************************************************************/