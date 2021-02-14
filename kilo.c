/*
 * 参考サイト: Build Your Own Text Editor
 * https://viewsourcecode.org/snaptoken/kilo/index.html
 */

/*** includes ***/
#include "kilo.h"
#include "terminal.h"

/*** defines ***/

#define KILO_VERSION "0.0.1"

#define CTRL_KEY(k) ((k) & 0x1f)

enum editorKey {
  ARROW_LEFT = 1000,
  ARROW_RIGHT,
  ARROW_UP,
  ARROW_DOWN,
  DELETE_KEY,
  HOME_KEY,
  END_KEY,
  PAGE_UP,
  PAGE_DOWN
};

/*** data ***/

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

/*** file i/o ***/

void editorOpen(char *filename) {
  FILE *fp = fopen(filename, "r");
  if (!fp) die("fopen");

  char *line = NULL;
  size_t linecapacity = 0;
  ssize_t linelen = getline(&line, &linecapacity, fp);
  if (linelen != -1) {
    while (linelen > 0 &&
      (line[linelen - 1] == '\n' || line[linelen - 1] == '\r')) linelen--;
    
    // malloc 使うならエラー処理しろよ
    E.row.size = linelen;
    E.row.chars = malloc(linelen + 1);
    memcpy(E.row.chars, line, linelen);
    E.row.chars[linelen] = '\0';
    E.numrows = 1;
  }

  free(line);
  fclose(fp);
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

void welcomeMessage(char *message, int limit) {
  char welcome[64];
  int welcomelen = snprintf(welcome, sizeof(welcome), "Kilo editor -- version %s", KILO_VERSION);
  if (welcomelen > limit) {
    strncpy(message, welcome, limit);
    message[limit] = '\0';
  } else {
    strncpy(message, welcome, welcomelen);
    message[welcomelen] = '\0';
  }
}

/**
 * 画面の一番左側1列を '~' で埋める
 */
void editorDrawRows(buffer_t *buffer) {
  const char ERASE_LINE[] = "\x1b[K";
  char welcome[64];
  welcomeMessage(welcome, E.screenclos);
  
  for (int y = 0; y < E.screenrows; y++) {
    if (y < E.numrows) {
      // このへんの処理共通化したい…
      int len = E.row.size;
      char linebuf[128];
      if (len > E.screenclos) {
        strncpy(linebuf, E.row.chars, E.screenclos);
        linebuf[E.screenclos] = '\0';
        bufferAppend(buffer, linebuf);
      } 
      bufferAppend(buffer, E.row.chars);
    } else if (E.numrows == 0 && y == E.screenrows / 3) {
      int padding = (E.screenclos - strlen(welcome)) / 2;
      if (padding > 0) {
        bufferAppend(buffer, "~");
        padding--;
        while (padding--) bufferAppend(buffer, " ");
      }
      bufferAppend(buffer, welcome);
    } else {
      bufferAppend(buffer, "~");
    }

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

  // カーソル位置を更新
  // パラメータに渡す座標は 1 から開始なので + 1 をする
  char cursor_position[32];
  snprintf(cursor_position, sizeof(cursor_position),
    "\x1b[%d;%dH", E.cursorY + 1, E.cursorX + 1);
  bufferAppend(&buffer, cursor_position);

  // 画面リフレッシュが終了したのでカーソルを見えるようにする
  bufferAppend(&buffer, CURSOR_VISIBLE);

  // バッファをすべて書きだす
  write(STDOUT_FILENO, buffer.string, buffer.length);
  bufferFree(&buffer);
}

/*** input ***/

void editorMoveCursor(int key) {
  switch (key) {
    case ARROW_LEFT:
      if (E.cursorX > 0) {
        E.cursorX--;
      }
      break;
    case ARROW_RIGHT:
      if (E.cursorX < E.screenclos - 1) {
        E.cursorX++;
      }
      break;
    case ARROW_UP:
      if (E.cursorY > 0) {
        E.cursorY--;
      }
      break;
    case ARROW_DOWN:
      if (E.cursorY < E.screenrows - 1) {
        E.cursorY++;
      }
      break;
  }
}

/**
 * 押したキーによって動作を分岐させる
 */
void editorProcessKeypress() {
  int c = editorReadkey();

  switch (c) {
    case CTRL_KEY('q'):
      write(STDOUT_FILENO, "\x1b[2J", 4);
      write(STDOUT_FILENO, "\x1b[H", 3);
      exit(0);
      break;
    
    case HOME_KEY:
      E.cursorX = 0;
      break;
    
    case END_KEY:
      E.cursorX = E.screenclos - 1;
      break;
    
    // ここの書き方あんまり好きじゃない
    case PAGE_UP:
    case PAGE_DOWN:
      {
        int times = E.screenrows;
        while (times--)
          editorMoveCursor(c == PAGE_UP ? ARROW_UP : ARROW_DOWN);
      }
      break;
    
    case ARROW_LEFT:
    case ARROW_RIGHT:
    case ARROW_UP:
    case ARROW_DOWN:
      editorMoveCursor(c);
      break;
  }
}

/*** init ***/

void initEditor() {
  E.cursorX = 0;
  E.cursorY = 0;
  E.numrows = 0;
  if (!getWindowSize(&E.screenrows, &E.screenclos)) die("getWindowSize");
}

int main(int argc, char *argv[]) {
  enableRawMode();
  initEditor();
  if (argc >= 2) {
    editorOpen(argv[1]);
  }

  while (1) {
    editorRefreshScreen();
    editorProcessKeypress();
  }
  return 0;
}