/******************************************************************************/
/*                                  MacroMix                                  */
/*                                                                            */
/*       A mix of most essential and basic chess programming techniques       */
/*          bundled into a single didactic CECP chess engine, by CMK          */
/*                                                                            */
/******************************************************************************/
/*                                  Contents                                  */
/*                                                                            */
/* - Compact move generator inspired by H.G. Muller's micro-Max               */
/* - Negamax search with alpha-beta inspired by Tom Kerrigan's TSCP           */
/* - Simplified evaluation function proposed by Tomasz Michniewski            */
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
  int side, ep, castle, hash;                     /* Position state snapshot  */
} Game;                                           /*                          */
/******************************************************************************/
Game game[1024];                                  /* Game moves history       */
int gamePly;                                      /* Game moves history ply   */
int nodes;                                        /* Number of positions      */ 
int board[128], side, ep, castle, hash;           /* Board position & state   */
int hashPiece[2][7][120];                         /* Random hash numbers      */
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
int force;                                        /* CECP variables           */
/******************************************************************************/
int material[] = { /* Piece weights */ 0, 20000, 100, 320, 330, 500, 900 }; /**/
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
/*                               Move generator                               */
/*                                                                            */
/******************************************************************************/
void printBoard() {                               /* Print board              */
  char p[] = ".kpnbrq--KPNBRQ";                   /* ASCII piece letters      */
  for (int rank = 0; rank < 8; rank++) {          /* Loop over board ranks    */
    for (int file = 0; file < 16; file++) {       /* Loop over board files    */
      int sq = rank * 16 + file;                  /* Calculate current square */
      if (file == 0) printf(" %d  ", 8 - rank);   /* Print ranks              */
      if (!(sq & 0x88)) {                         /* Square is not offboard   */
        printf("%c ", p[board[sq] & 15]);         /* Print board square       */
      }                                           /*                          */
    } printf("\n");                               /* Print new line           */
  } printf("\n    a b c d e f g h\n\n");          /* Print files              */
}                                                 /*                          */
/******************************************************************************/
int attack(int sq, int col) {                     /* Square attacked by color */
  for (int pceType = 1; pceType < 7; pceType++) { /* Loop over piece types    */
    int pce = pceType | col;                      /* Build piece              */
    if (pceType == 2) {                           /* Attacked by a pawn       */
      int dir = ((col == 8) ? 16 : -16);          /* Attack direction         */
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
                if (att == pce)                   /* Piece attacks square     */
                  return 1;                       /* Return true              */
                break;                            /* Search no more           */
            }                                     /*                          */
        } while (slider);                         /* Repeat for slider pieces */
      }                                           /*                          */
    }                                             /*                          */
  } return 0;                                     /* Square is not attacked   */
}                                                 /*                          */
/******************************************************************************/
void genMoves(MList *mList, int caps) {           /* Generate moves/captures  */
  if (mList) mList->count = 0;                    /* Reset move count         */
  for (int src = 0; src < 120; src++) {           /* Loop over board squares  */
    int pce = board[src];                         /* Get square contents      */
    if (src & 0x88) continue;                     /* Skip offboard squares    */
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
              capPce == 0 && capSq != ep) break;  /* Stop otherwise           */
        } if (pceType == 2 &&                     /* Piece is a pawn          */
          ((dst >> 4) == (side == 8 ? 0 : 7))) {  /* Pawn is promoting        */
          for (int prom = 6; prom > 2; prom--) {  /* Loop over promotoins     */
          if (caps == 0)                          /* Generate all moves       */
          mList->moves[mList->count++] = (Move) { /* Add all promotion        */
            src, dst, capSq, pce, capPce, prom, 0 /* Move contents            */
          }; else if (caps && capPce)             /* Generate only captures   */
          mList->moves[mList->count++] = (Move) { /* Add only captures        */
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
int inCheck(int col) {                            /* Is king in check ?       */
  for (int sq = 0; sq < 120; sq++) {              /* Loop over board squares  */
    if (sq & 0x88) continue;                      /* Skip offboard squares    */
    int pce = board[sq];                          /* Get square contents      */
    if ((pce & 7) == 1 && (pce & col))            /* King of side to move     */
      return attack(sq, 24 - col);                /* Attacked by other side?  */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
void setHash() {                                  /* Used for repetition      */
  hash = 0;                                       /* Reset position hash key  */
  for (int sq = 0; sq < 120; sq++) {              /* Loop over board squares  */
    if (sq & 0x88) continue;                      /* Skip offboard squares    */
    int pce = board[sq];                          /* Get square contents      */
    int col = -1;                                 /* Set no piece color       */
    if (pce & 8) col = 0;                         /* Piece is white           */
    if (pce & 16) col = 1;                        /* Piece is black           */
    if (col >= 0)                                 /* Piece is not empty       */
      hash ^= hashPiece[col][pce & 7][sq];        /* Hash piece               */
  }                                               /*                          */
}                                                 /*                          */
/******************************************************************************/
void undo() {                                     /* Undo move on board       */
  gamePly--;                                      /* Decrement game ply       */
  Move move = game[gamePly].move;                 /* Load move from a game    */
  side = game[gamePly].side;                      /* Load side to move        */
  ep = game[gamePly].ep;                          /* Load en passant square   */
  castle = game[gamePly].castle;                  /* Load castling rights     */
  hash = game[gamePly].hash;                      /* Load hash key            */
  board[move.dst] = 0;                            /* Empty target square      */
  board[move.src] = move.pce;                     /* Move a piece back        */
  board[move.capSq] = move.capPce;                /* Restore captured piece   */
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
int make(Move move) {                             /* Make move on board       */
  game[gamePly].move = move;                      /* Save move into a game    */
  game[gamePly].side = side;                      /* Save side to move        */
  game[gamePly].ep = ep;                          /* Save en passant square   */
  game[gamePly].castle = castle;                  /* Save castling rights     */
  game[gamePly].hash = hash;                      /* Save hash key            */
  gamePly++;                                      /* Increment game ply       */
  board[move.capSq] = 0;                          /* Empty captured square    */
  board[move.src] = 0;                            /* Empty source square      */
  board[move.dst] = move.pce;                     /* Move piece               */
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
  side = 24 - side;                               /* Swap the side to move    */
  if (inCheck(24 - side)) {                       /* King in check after move */
    undo();                                       /* Take move back           */
    return 0;                                     /* Return illegal move      */
  } setHash();                                    /* Update position hash     */
  return 1;                                       /* Return legal move        */
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
/*                                 Evaluation                                 */
/*                                                                            */
/******************************************************************************/
int eval() {                                      /* Static evaluation        */
  int score = 0;                                  /* Reset static score       */
  int matWhite = 0, matBlack = 0;
  for (int sq = 0; sq < 120; sq++) {              /* Loop over board squares  */
    if (sq & 0x88) continue;                      /* Skip offboard square     */
    int pce = board[sq];                          /* Get square contents      */
    int pceType = pce & 7;                        /* Extract piece type       */
    if (pceType == 1) continue;                   /* Skip kings               */
    if (pce & 8) matWhite += material[pceType];   /* Count white pieces       */
    if (pce & 16) matBlack += material[pceType];  /* Count black pieces       */
  } for (int sq = 0; sq < 120; sq++) {            /* Loop over board squares  */
    if (sq & 0x88) continue;                      /* Skip offboard square     */
    int pce = board[sq];                          /* Get square contents      */
    if (pce) {                                    /* Found a piece            */
      int pceType = pce & 7;                      /* Extract piece type       */
      int mat = material[pceType];                /* Material score           */
      int pos;                                    /* Positional score         */
      int s = sq;                                 /* Opening/Endgame square   */
      if (pce & 8) {                              /* White piece              */
        if (matWhite < 1200 && pceType == 1)      /* In an endgame            */
          s += 8;                                 /* Use king endgame scores  */
        pos = pst[pceType][s];                    /* White positional score   */
      } if (pce & 16) {                           /* Black piece              */
        if (matBlack < 1200 && pceType == 1)      /* In an endgame            */
          s += 8;                                 /* Use king endgame scores  */
        pos = pst[pceType][s ^ 112];              /* Black positional score   */
      } int value = mat + pos;                    /* Total score              */
      score += (pce & 8) ? value : -value;        /* Aggregate score          */
    }                                             /*                          */
  } return (side == 8) ? score : -score;          /* Side respective score    */
}                                                 /*                          */
/******************************************************************************/
/*                               CECP Protocol                                */
/*                                                                            */
/******************************************************************************/
int main() {                                      /* Xboard / Winboard driver */
  signal(SIGINT, SIG_IGN);                        /* Ignore INTERRUPT signal  */
  printf("MacroMix by Code Monkey King\n");       /* Engine info for GUI      */
  char input[80] = "new";                         /* Run "new" to init engine */
  setbuf(stdout, NULL);                           /* Clear output buffer      */
  do {                                            /* User command loop        */
    char command[32] = { 0 };                     /* User command storage     */                 
    sscanf(input, "%31[^\n]", command);           /* Read user command        */
    if (!strcmp(command, "new")) {                /* Command "new"            */
      srand(0);                                   /* Set random seed          */
      for (int col = 0; col < 2; col++)           /* Loop over color types    */
        for (int t = 0; t < 7; t++)               /* Loop over piece types    */
          for (int sq = 0; sq < 120; sq++)        /* Loop over board squares  */
            hashPiece[col][t][sq] = rand();       /* Init random hash number  */
      side = 8; ep = -1; castle = 15;             /* Init position state      */
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
        setHash();                                /* Hash position            */
        continue;                                 /* Process next command     */
    } if (!strcmp(command, "white")) {            /* Command "white"          */
      force = 0; side = 8; continue;              /* Computer plays black     */
    } if (!strcmp(command, "black")) {            /* Command "black"          */
      force = 0; side = 16; continue;             /* Computer plays white     */
    } if (!strcmp(command, "force")) {            /* Command "force"          */
      force = 1; continue;                        /* Computer plays neither   */
    } if (command[0] >= 'a' &&                    /*                          */
          command[0] <= 'z' &&                    /* If command is a move     */
          command[1] >= '0' &&                    /* in format "e2e4"         */
          command[1] <= '9') {                    /*                          */
      int srcFile = command[0] - 'a';             /* Extract source file      */
      int srcRank = 8 - (command[1] - '0');       /* Extract source rank      */
      int dstFile = command[2] - 'a';             /* Extract target file      */
      int dstRank = 8 - (command[3] - '0');       /* Extract target rank      */
      int src = srcRank * 16 + srcFile;           /* Calculate source square  */
      int dst = dstRank * 16 + dstFile;           /* Calculate target square  */
      int prom = command[4] & 0x5f;               /* Extract promoted piece   */
      if (prom) {                                 /* Promoted piece available */
        switch (prom) {                           /* Process promoted piece   */
          case 'Q': prom = 6;                     /* Promoted piece is Queen  */
          case 'R': prom = 5;                     /* Promoted piece is Rook   */
          case 'B': prom = 4;                     /* Promoted piece is Bishop */
          case 'N': prom = 3;                     /* Promoted piece is Knight */
          default: prom = 6;                      /* Queen is default         */
        }                                         /*                          */
      } MList mList; genMoves(&mList, 0);         /* Generate moves           */
      for (int i = 0; i < mList.count; i++) {     /* Loop over moves          */
        Move move = mList.moves[i];               /* Get current move         */
        if (src == move.src &&                    /*                          */
            dst == move.dst &&                    /* User move is in the list */
            prom == move.prom) {                  /*                          */
          make(move); break;                      /* Make move if legal       */
        }                                         /*                          */
      }                                           /*                          */
      if (force == 0) {                           /* Search best move         */
      } continue;                                 /*                          */
    } if (!strcmp(command, "undo")) {             /* Command "undo"           */
      if (gamePly > 0) undo(); continue;          /* Undo move if any         */
    } if (!strncmp(command, "perft", 5)) {        /* Perft test               */
      int tick = getTimeMs();                     /* Start timer              */
      nodes = 0;                                  /* Reset node count         */
      int d = atoi(command + 6);                  /* Parse perft depth        */
      printf("Perft depth %d ", d); perft(d);     /* Run test                 */
      int time = getTimeMs() - tick;              /* Stop timer               */
      printf("nodes %d time %d\n", nodes, time);  /* Print result             */
      continue;                                   /* Read next command        */
    } if (!strcmp(command, "board")) {            /* Extra command "board"    */
      printBoard(); continue;                     /* Print board to console   */
    } if (!strcmp(command, "quit")) break;        /* Terminate engine         */
  }                                               /*                          */
  while (fgets(input, sizeof(input), stdin));     /* Read user input          */
  return 0;                                       /* Exit program             */
}                                                 /*                          */
/******************************************************************************/