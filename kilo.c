#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <unistd.h>

struct termios orig_termios;

void disableRawMode() {
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
}

void enableRawMode() {
  tcgetattr(STDIN_FILENO, &orig_termios);
  atexit(disableRawMode);

  struct termios raw = orig_termios;

  // c_iflag: input
  // IXON: 出力の XON/XOFF フロー制御を有効にする
  // → ctrl-s(XON), ctrl-q(XOFF) を無効にする
  // c_flag: local flag
  // ECHO: 入力をエコーする
  // ICANON: 入力を行単位に設定
  // ISIG: INTR, QUIT, SUSP, DSUSP の文字を受信した時、対応するシグナルを 発生させる
  // これらのフラグの論理和を取って否定する
  // → echo しないようにしてる
  // → 入力はバイト単位
  // → シグナルが発生しなくなる、つまりctrl-c, ctrl-z が入力されても動く
  raw.c_iflag &= ~(IXON);
  raw.c_lflag &= ~(ECHO | ICANON | ISIG);

  // TCSAFLUSH: fd に書き込まれたすべての出力が端末に送信された後で変更が実行される.
  // 加えて、受信したが読まれなかった入力は捨てられる.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();

  char c;
  while (read(STDIN_FILENO, &c, 1) == 1 && c != 'q') {
    if (iscntrl(c)) {
      // control charactor: ASCII code 0-31, 127
      printf("%03d\n", c);
    } else {
      // ASCII code 32-126
      printf("%03d ('%c')\n", c, c);
    }
  }
  return 0;
}