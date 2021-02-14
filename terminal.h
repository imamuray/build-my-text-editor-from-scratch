#ifndef TERMINAL_H
#define TERMINAL_H

void disableRawMode();
void enableRawMode();
int editorReadkey();
bool getCursorPosition(int *rows, int *cols);
bool getWindowSize(int *rows, int *cols);

#endif