#include "kilo.h"
#include "terminal.h"

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