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

  /*
   * c_iflag: input
   * IXON: 出力の XON/XOFF フロー制御を有効にする
   * ICRNL: 入力の CR('\r') を NL('\n') に置き換える
   * → ctrl-s(XON), ctrl-q(XOFF) を無効にする
   * → ctrl-m が変換されずに 13 と表示される
   * 
   * c_oflag: output
   * OPOST: 実装に依存した出力処理を有効にする
   * → 無効にする
   * 
   * c_flag: local flag
   * ECHO: 入力をエコーする
   * ICANON: 入力を行単位に設定
   * IEXTEN: 実装依存の入力処理を有効にする
   * ISIG: INTR, QUIT, SUSP, DSUSP の文字を受信した時、対応するシグナルを 発生させる
   * これらのフラグの論理和を取って否定する
   * → echo しないようにしてる
   * → 入力はバイト単位
   * → ctrl-x を入力できるようにする
   * → シグナルが発生しなくなる、つまりctrl-c, ctrl-z が入力されても動く
   * 
   * c_cc: control characters
   * VMIN: read() が返るまでに必要な最小のバイト数
   * → 0 でも返ってこれる
   * VTIME: read() が返るまでに待つ最大の時間
   * → 1/10 秒単位なので、100 ミリ秒
   * read() が短い時間で 0 でも返すようにしているのは、入力待ちをなくすためだと思われる
   */
  raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
  raw.c_oflag &= ~(OPOST);
  raw.c_cflag |= (CS8);
  raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;

  // TCSAFLUSH: fd に書き込まれたすべての出力が端末に送信された後で変更が実行される.
  // 加えて、受信したが読まれなかった入力は捨てられる.
  tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

int main() {
  enableRawMode();

  while (1) {
    char c = '\0';
    read(STDIN_FILENO, &c, 1);
    if (iscntrl(c)) {
      // control charactor: ASCII code 0-31, 127
      printf("%03d\r\n", c);
    } else {
      // ASCII code 32-126
      printf("%03d ('%c')\r\n", c, c);
    }
    if (c == 'q') break;
  }
  return 0;
}