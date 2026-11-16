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
int board[128], side, ep, castle;                 /* Board position & state   */
int offsets[7][8] = {                             /* Piece move offsets       */
  { 0 },                                          /* ------------------------ */
  { -16, -1, 16, 1, -17, -15, 17, 15 },           /* King                     */
  { 15, 16, 17 },                                 /* Pawn                     */
  { -33, -31, 33, 31, -18, -14, 18, 14 },         /* Knight                   */
  { -17, -15, 17, 15 },                           /* Bishop                   */
  {-16, -1, 16, 1 },                              /* Rook                     */
  { -16, -1, 16, 1, -17, -15, 17, 15 }            /* Queen                    */
};                                                /* ------------------------ */

void print() { // TEMP
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
  printf("side %d;  ep %d;  castle %d\n", side, ep, castle);
}

int movegen(MList *mList, int caps) {
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
  } return 0;
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
        continue;                                 /* Process next command     */
    }
    if (!strcmp(command, "quit")) break;          /* Terminate engine         */
    if (!strcmp(command, "print")) {
      print();                                    /* TEMP                     */
      MList mList; movegen(&mList, 0);
      
      for (int i = 0; i < mList.count; i++) {
        Move move = mList.moves[i];
        printf("%c%c%c%c%c\n",
          (move.src & 7) + 'a',
          8 - ((move.src >> 4) & 7) + '0',
          (move.dst & 7) + 'a',
          8 - ((move.dst >> 4) & 7) + '0',
          move.prom ? "---NBRQ-"[move.prom] : ' '
        );
      }
      
    }
  } while (fgets(input, sizeof(input), stdin));   /* Read user input          */
  return 0;                                       /* Exit program             */
}





