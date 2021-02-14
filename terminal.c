#include "kilo.h"

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1) die("tcsetattr");
}

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1) die("tcsetattr");
  atexit(disableRawMode);

  struct termios raw = E.orig_termios;

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
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
}

/**
 * ターミナルから1文字読み取る
 */
int editorReadkey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }

  if (c != '\x1b') {
    return c;
  } else {
    char seq[3];
    if (read(STDIN_FILENO, &seq[0], 1) != 1) return '\x1b';
    if (read(STDIN_FILENO, &seq[1], 1) != 1) return '\x1b';

    // このへん関数で切り出したい 
    if (seq[0] == '[') {
      if (isdigit(seq[1])) {
        if (read(STDIN_FILENO, &seq[2], 1) != 1) return '\x1b';
        if (seq[2] == '~') {
          switch (seq[1]) {
            case '1': return HOME_KEY;
            case '3': return DELETE_KEY;
            case '4': return END_KEY;
            case '5': return PAGE_UP;
            case '6': return PAGE_DOWN;
            case '7': return HOME_KEY;
            case '8': return END_KEY;
          }
        }
      } else {
        switch (seq[1]) {
          case 'A': return ARROW_UP;
          case 'B': return ARROW_DOWN;
          case 'C': return ARROW_RIGHT;
          case 'D': return ARROW_LEFT;
          case 'H': return HOME_KEY;
          case 'F': return END_KEY;
        }
      }
    } else if (seq[0] == 'O') {
      switch (seq[1]) {
        case 'H': return HOME_KEY;
        case 'F': return END_KEY; 
      }
    }
    return '\x1b';
  }
}

/**
 * rows, cols に現在のカーソル位置を設定する
 */
bool getCursorPosition(int *rows, int *cols) {
  char buf[32];
  unsigned int i = 0;

  // n: Device Status Report
  // 6: command from host
  // カーソル位置を取得する
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4) return false;
  // Cursor Position Report が返ってくる
  // ESC [ line ; column R
  while (i < sizeof(buf) - 1) {
    if (read(STDIN_FILENO, &buf[i], 1) != 1) break;
    if (buf[i] == 'R') break;
    i++;
  }
  // 最後の 'R' を落とす
  buf[i] = '\0';

  // "ESC [ line ; column" をパースする
  if (buf[0] != '\x1b' || buf[1] != '[') return false;
  // sscanf() は入力データの個数を返す
  if (sscanf(&buf[2], "%d;%d", rows, cols) != 2) return false;

  return true;
}

/**
 * rows, cols にウィンドウサイズを設定する
 */
bool getWindowSize(int *rows, int *cols) {
  struct winsize ws;

  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
    // ioctrl() はすべてのシステムでサイズをとってこれるとは限らないので別の方法
    // ESC [ Pn C: Cursor Forward
    // ESC [ Pn B: Cursor Down
    // カーソル移動には H もあるが、ウィンドウサイズ以上の動作は未定義なので使用していない
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12) return false;
    return getCursorPosition(rows, cols);
  }

  *cols = ws.ws_col;
  *rows = ws.ws_row;
  return true;
}