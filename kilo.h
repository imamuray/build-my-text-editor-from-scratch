#define _DEFAULT_SOURCE
#define _BSD_SOURCE
#define _GNU_SOURCE

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <termios.h>
#include <unistd.h>

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

/**
 * Editor row
 * ある行のテキストとその文字数を保持する
 */
typedef struct erow {
  int size;
  char *chars;
} erow;

struct editorConfig {
  int cursorX, cursorY;
  int screenrows;
  int screenclos;
  int numrows;
  erow row;
  struct termios orig_termios;
};

struct editorConfig E;

void die(const char *s);