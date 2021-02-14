/*
 * 参考サイト: Build Your Own Text Editor
 * https://viewsourcecode.org/snaptoken/kilo/index.html
 */

/*** includes ***/
#include "kilo.h"
#include "terminal.h"

/**
 * エラーメッセージを表示してプログラムを終了する
 */
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

/*** terminal ***/
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