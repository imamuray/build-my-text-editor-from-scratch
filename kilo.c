/*
 * 参考サイト: Build Your Own Text Editor
 * https://viewsourcecode.org/snaptoken/kilo/index.html
 */

/*** includes ***/
#include "kilo.h"
#include "terminal.h"
#include "fileIO.h"
#include "input.h"
#include "output.h"

/**
 * エラーメッセージを表示してプログラムを終了する
 */
void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
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