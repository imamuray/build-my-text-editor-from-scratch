#include "kilo.h"

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