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
typedef struct {                                  /* ------------------------ */
  int src, dst, capSq;                            /* Move & captured squares  */
  int pce, capPce;                                /* Attacker & victim pieces */ 
  int prom;                                       /* Pawn promotion piece     */
  int score;                                      /* Score for move ordering  */
} Move;                                           /* ------------------------ */
                                                  /*                          */
typedef struct {                                  /* ------------------------ */
  Move moves[256];                                /* Moves in a list          */
  int count;                                      /* Move list length         */
} MList;                                          /* ------------------------ */
                                                  /*                          */
typedef struct {                                  /* ------------------------ */
  Move move;                                      /* Game move                */
  int side, ep, castle, hash;                     /* Position state snapshot  */
} Game;                                           /* ------------------------ */
                                                  /*                          */
Game game[1024]; int gamePly;                     /* Game moves & current ply */
int force;
int board[128], side, ep, castle, hash;           /* Board position & state   */
int hashPiece[2][7][120];                         /* Random hash numbers      */
int offsets[7][8] = {                             /* Piece move offsets       */
  { 0 },                                          /* ------------------------ */
  { -16, -1, 16, 1, -17, -15, 17, 15 },           /* King                     */
  { 15, 16, 17 },                                 /* Pawn                     */
  { -33, -31, 33, 31, -18, -14, 18, 14 },         /* Knight                   */
  { -17, -15, 17, 15 },                           /* Bishop                   */
  {-16, -1, 16, 1 },                              /* Rook                     */
  { -16, -1, 16, 1, -17, -15, 17, 15 }            /* Queen                    */
};                                                /* ------------------------ */

void printb() { // TEMP
  char p[] = ".kpnbrq--KPNBRQ";
  for (int rank = 0; rank < 8; rank++) {
    for (int file = 0; file < 16; file++) {
      int sq = rank * 16 + file;
      if (file == 0) printf(" %d  ", 8 - rank);
      if (!(sq & 0x88)) {
        printf("%c ", p[board[sq] & 15]);
      }
    } printf("\n");
  } printf("\n    a b c d e f g h\n\n");
  printf("side %d\nep %d\ncastle %d\nhash %d\ngamePly %d\n", side, ep, castle, hash, gamePly);
}

void setHash() {
  hash = 0;
  for (int sq = 0; sq < 120; sq++) {
    if (sq & 0x88) continue;
    int pce = board[sq], col = -1;
    if (pce & 8) col = 0;
    if (pce & 16) col = 1;
    if (col >= 0) hash ^= hashPiece[col][pce & 7][sq];
  }
}

void genMoves(MList *mList, int caps) {
  if (mList) mList->count = 0;
  for (int src = 0; src < 120; src++) {
    int pce = board[src];
    if (!(pce & side) || (src & 0x88)) continue;
    int pceType = pce & 7;
    for (int i = 0; i < 8; i++) {
      int dir = offsets[pceType][i];
      if (dir == 0) break;
      if (pce == 10) dir *= -1;
      int dst = src + dir;
      while (!(dst & 0x88)) {
        int capSq = dst;
        if (pceType == 2 && dst == ep) capSq ^= 16;
        int capPce = board[capSq];
        if (capPce & side) break;
        if (pceType == 2) {
          if (!((dst - src) & 7) && capPce) break;
          if (((dst - src) & 7) && !capPce && capSq != ep) break;
        } if (pceType == 2 && ((dst >> 4) == (side == 8 ? 0 : 7))) {
          for (int prom = 6; prom > 2; prom--) {
            mList->moves[mList->count++] = (Move) {
              src, dst, capSq, pce, capPce, prom, 0
            };
          }
        } else mList->moves[mList->count++] = (Move) {
          src, dst, capSq, pce, capPce, 0, 0
        };if (pceType == 2) {
          if ((src >> 4) != ((side == 8) ? 6 : 1)) break;
          if ((dst >> 4) != ((side == 8) ? 5 : 2)) break;
        } else if (pceType == 1) {
          if ((dir != 1) && (dir != -1)) break;
          if (dir == 1) {
            if (board[src + 2]) break;
            if (side == 8 && !(castle & 1)) break;
            if (side == 16 && !(castle & 4)) break;
          } if (dir == -1) {
            if (board[src - 2]) break;
            if (board[src - 3]) break;
            if (side == 8 && !(castle & 2)) break;
            if (side == 16 && !(castle & 8)) break;
          } if (src != ((side == 8) ? 116: 4)) break;
          if (abs(src-dst) > 1) break;
        } if (capPce & (24 - side)) break;
        if (!(pceType & 4) && pceType != 2 && pceType != 1) break;
        dst += dir; 
      }
    }
  }
}

int attack(int sq, int col) {
  for (int pceType = 1; pceType <= 6; pceType++) {
    int pce = pceType | col;
    if (pceType == 2) {
      int direction = 16 * (1 - 2 * col);
      for (int cap = -1; cap <= 1; cap += 2) {
        int dst = sq + direction + cap;
        if (!(dst & 0x88) && board[dst] == pce) return 1;
      }
    } else {
      int slider = pceType & 0x04;
      for (int d = 0; d < 8; d++) {
        int dir = offsets[pceType][d];
        if (dir == 0) break;
        int dst = sq;
        do {
            dst += dir;
            if (dst & 0x88) break;
            int att = board[dst];
            if (att != 0) {
                if (att == pce) return 1;
                break;
            }
        } while (slider);
      } 
    }
  } return 0;
}

int inCheck(int col) {
  for (int sq = 0; sq < 120; sq++) {
    int pce = board[sq];
    if ((pce & 7) == 1 && (pce & col))
      return attack(sq, 24 - col);
  }
}

void undo() {
  gamePly--;
  Move move = game[gamePly].move;
  side = game[gamePly].side;
  ep = game[gamePly].ep;
  castle = game[gamePly].castle;
  hash = game[gamePly].hash;
  board[move.dst] = 0;
  board[move.src] = move.pce;
  board[move.capSq] = move.capPce;
  if ((move.pce & 7) == 1 &&
     ((move.src == 116) || (move.src == 4))) {
    switch(move.dst) {
      case 118: board[119] = 13; board[117] = 0; break;
      case 114: board[112] = 13; board[115] = 0; break;
      case 6: board[7] = 21; board[5] = 0; break;
      case 2: board[0] = 21; board[3] = 0; break;
    }
  } 
}

int make(Move move) {
  board[move.capSq] = 0;
  board[move.src] = 0;
  board[move.dst] = move.pce;
  
  if ((move.pce & 7) == 1 &&
     ((move.src == 116) || (move.src == 4))) {
    switch(move.dst) {
      case 118:
        board[117] = 13; board[119] = 0; break;
      case 114:
        board[115] = 13; board[112] = 0; break;
      case 6:
        board[5] = 21; board[7] = 0; break;
      case 2:
        board[3] = 21; board[0] = 0; break;
    }
  }
  
  game[gamePly].move = move;
  game[gamePly].side = side;
  game[gamePly].ep = ep;
  game[gamePly].castle = castle;
  game[gamePly].hash = hash;
  gamePly++;

  castle &= board[move.src + 8];
  castle &= board[move.dst + 8];

  ep = -1;
  if ((move.pce & 7) == 2) {
    if (
      ((move.src >> 4) == ((side == 8) ? 6 : 1)) &&
      ((move.dst >> 4) == ((side == 8) ? 4 : 3))
    ) ep = move.dst ^ 16;
    if ((move.dst >> 4) == (side == 8 ? 0 : 7)) {
      board[move.dst] = move.prom | side;
    }
  }
  
  side = 24 - side;
  
  if (inCheck(24 - side)) {
    undo();
    return 0;
  }
  
  setHash();
  return 1;
}


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
        Move move = mList.moves[i];               /* Init move                */
        if (src == move.src &&                    /*                          */
            dst == move.dst &&                    /* User move is in the list */
            prom == move.prom) {                  /*                          */
          make(move); break;                      /* Try making a move        */
        }
      } printb();
      if (force == 0) {
        // search move
      } continue;
    } if (!strcmp(command, "undo")) {             /* Command "undo"           */
      if (gamePly > 0) undo(); continue;          /* Undo move if any         */
    }
    
    
    
    if (!strcmp(command, "quit")) break;          /* Terminate engine         */
    if (!strcmp(command, "board")) {              /* Print board              */
      /*MList mList; movegen(&mList, 0);
      
      for (int i = 0; i < mList.count; i++) {
        Move move = mList.moves[i];
        make(&move); //print(); getchar();
        undo(&move); //print(); getchar();
        printf("%c%c%c%c%c\n",
          (move.src & 7) + 'a',
          8 - ((move.src >> 4) & 7) + '0',
          (move.dst & 7) + 'a',
          8 - ((move.dst >> 4) & 7) + '0',
          move.prom ? "---NBRQ-"[move.prom] : ' '
        );
      }*/
      
      printb(); continue;
      
    }
  } while (fgets(input, sizeof(input), stdin));   /* Read user input          */
  return 0;                                       /* Exit program             */
}





