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
int board[128], side, ep, castle;                 /* Board position & state   */
                                                  /*                          */
int offsets[7][8] = {                             /* Piece move offsets       */
  { 0 },                                          /*                          */
  { -16, -1, 16, 1, -17, -15, 17, 15 },           /* King                     */
  { 15, 16, 17 },                                 /* Pawn                     */
  { -33, -31, 33, 31, -18, -14, 18, 14 },         /* Knight                   */
  { -17, -15, 17, 15},                            /* Bishop                   */
  {-16, -1, 16, 1 },                              /* Rook                     */
  { -16, -1, 16, 1, -17, -15, 17, 15 }            /* Queen                    */
};                                                /*                          */


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
    if (!strcmp(command, "print")) print();       /* TEMP                     */
  } while (fgets(input, sizeof(input), stdin));   /* Read user input          */
  return 0;                                       /* Exit program             */
}