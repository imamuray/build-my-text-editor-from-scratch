/*
 * 参考サイト: Build Your Own Text Editor
 * https://viewsourcecode.org/snaptoken/kilo/index.html
 */

/*** includes ***/

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*** defines ***/

#define CTRL_KEY(k) ((k) & 0x1f)

/*** data ***/

struct editorConfig {
  int screenrows;
  int screenclos;
  struct termios orig_termios;
};

struct editorConfig E;

/*** terminal ***/

/**
 * エラーメッセージを表示してプログラムを終了する
 */
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

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
char editorReadkey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1)) != 1) {
    if (nread == -1 && errno != EAGAIN) die("read");
  }
  return c;
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

/*** append buffer ***/

typedef struct {
  char *string;
  int length;
} buffer_t;

#define BUFFER_INIT {NULL, 0}

/**
 * src->length + length のメモリを確保してから、
 * src に string を加える
 */
void bufferAppend(buffer_t *src, const char *string) {
  int length = strlen(string);
  char *new = realloc(src->string, src->length + length);

  // 失敗を呼び出し元に知らせなくていいんですか???
  if (new == NULL) return;
  memcpy(&new[src->length], string, length);
  src->string = new;
  src->length += length;
}

void bufferFree(buffer_t *buffer) {
  free(buffer->string);
}

/*** output ***/

/**
 * 画面の一番左側1列を '~' で埋める
 */
void editorDrawRows(buffer_t *buffer) {
  const char ERASE_LINE[] = "\x1b[K";

  for (int y = 0; y < E.screenrows; y++) {
    bufferAppend(buffer, "~");
    bufferAppend(buffer, ERASE_LINE);

    if (y < E.screenrows - 1) {
      // 最後の行にも "~" を表示するため
      // 最後も改行してしまうと、端末が改行文字出力のために上にスクロールしてしまう
      bufferAppend(buffer, "\r\n");
    }
  }
}

// VT100 のマニュアル
// Digital VT100 User Guide: Programmer Information
// https://vt100.net/docs/vt100-ug/chapter3.html

void editorRefreshScreen() {
  const char CURSOR_VISIBLE[] = "\x1b[?25h";
  const char CURSOR_INVISIBLE[] = "\x1b[?25l";
  const char RETURN_CURSOR_TO_HOME[] = "\x1b[H";

  buffer_t buffer = BUFFER_INIT;

  // リフレッシュ時にカーソルが画面中央でちらつく可能性があるので、一旦見えなくする
  bufferAppend(&buffer, CURSOR_INVISIBLE);

  // 画面リフレッシュ開始
  bufferAppend(&buffer, RETURN_CURSOR_TO_HOME);

  editorDrawRows(&buffer);

  bufferAppend(&buffer, RETURN_CURSOR_TO_HOME);

  // 画面リフレッシュが終了したのでカーソルを見えるようにする
  bufferAppend(&buffer, CURSOR_VISIBLE);

  // バッファをすべて書きだす
  write(STDOUT_FILENO, buffer.string, buffer.length);
  bufferFree(&buffer);
}

/*** input ***/

/**
 * 押したキーによって動作を分岐させる
 */
void editorProcessKeypress() {
  char c = editorReadkey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
  }
}

/*** init ***/

void initEditor() {
  if (!getWindowSize(&E.screenrows, &E.screenclos)) die("getWindowSize");
}

int main() {
  enableRawMode();
  initEditor();

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}