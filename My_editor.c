#include <asm-generic/ioctls.h>
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>

/*
 *
 *  Some macro definitos here
 */
#define CTRL_KEY(k) ((k) & 0x1f)
#define EDITOR_VERSION "0.0.0.0.0"

/*
   data here
*/
struct editorConfig {
  int screenrows;
  int screencols;
  struct termios orig_termios;
};
struct editorConfig E;

/*
   Terminal functions
        */

void die(const char *s) {
  write(STDOUT_FILENO, "\x1b[2J", 4);
  write(STDOUT_FILENO, "\x1b[H", 3);

  perror(s);
  exit(1);
}

void disableRawMode() {
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &E.orig_termios) == -1)
    die("tcsetattr");
} // this disables raw mode by restoring the original setting

void enableRawMode() {
  if (tcgetattr(STDIN_FILENO, &E.orig_termios) == -1)
    die("tcsetattr");
  atexit(disableRawMode);
  struct termios raw = E.orig_termios;
  tcgetattr(STDIN_FILENO, &raw);
  raw.c_iflag &= ~(BRKINT | IXON | ICRNL | INPCK | ISTRIP);
  raw.c_oflag &= ~(OPOST); // it changes the carriage retrun thing
  raw.c_lflag &= ~(ICANON | ECHO |
                   ISIG); // changes the echo flag and we are also disabling
                          // cannonical mode also block now ctrl+c
  raw.c_cflag |= (CS8);
  raw.c_cc[VMIN] = 0;
  raw.c_cc[VTIME] = 1;
  if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1)
    die("tcsetattr");
}

char editorReadKey() {
  int nread;
  char c;
  while ((nread = read(STDIN_FILENO, &c, 1) != 1)) {
    if (nread == -1 && errno != EAGAIN)
      die("read");
  }
  return c;
}

int getCursorPosition(int *rows, int *cols) {
  char buff[32];
  unsigned int i = 0;
  if (write(STDOUT_FILENO, "\x1b[6n", 4) != 4)
    return -1;
  while (i < sizeof(buff) - 1) {
    if (read(STDIN_FILENO, &buff[i], 1) != 1)
      break;
    if (buff[i] == 'R') {
      break;
    }
    i++;
  }
  buff[i] = '\0';
  if (buff[0] != '\x1b' || buff[1] != '[')
    return 1;
  if (sscanf(&buff[2], "%d;%d", rows, cols) != 2)
    return -1;

  return 0;
}

int getWindowSize(int *rows, int *cols) {
  struct winsize ws;
  if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 ||
      ws.ws_col == 0) { // if ioctl fails we are using manual method to check
                        // the window size by placing cursor at the bottom right
    if (write(STDOUT_FILENO, "\x1b[999C\x1b[999B", 12) != 12)
      return -1;
    return getCursorPosition(rows, cols);
  } else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
  }
  return 0;
}

/* Appending buffer in this section*/

struct abuf {
  char *b;
  int len;
};

#define ABUF_INIT                                                              \
  { NULL, 0 }

void abAppend(struct abuf *ab, const char *s, int len) {
  char *new = realloc(ab->b, ab->len + len);
  if (new == NULL)
    return;
  memcpy(&new[ab->len], s, len);
  ab->b = new;
  ab->len += len;
}

void abFree(struct abuf *ab) { free(ab->b); }

/*
 * handling input
 * */

void editorProcessKeypress() {
  char c = editorReadKey();
  switch (c) {

  case CTRL_KEY('q'):
    write(STDOUT_FILENO, "\x1b[2J", 4);
    write(STDOUT_FILENO, "\x1b[H", 3);
    exit(0);
    break;
  }
}

/*
 *    Output handling section
 *
 *
 */
void editorDrawRows(struct abuf *ab) {
  for (int i = 0; i < E.screenrows; i++) {
    if (i == E.screenrows / 3) {
      char welcome[88];
      int welcomelen = snprintf(welcome, sizeof(welcome),
                                "My_editor ---version %s", EDITOR_VERSION);
      if (welcomelen > E.screencols) {
        welcomelen = E.screencols;
      }
      int padding = (E.screencols - welcomelen) / 2;
      if (padding) {
        abAppend(ab, "*", 1);
        padding--;
      }
      while (padding) {
        abAppend(ab, " ", 1);
        padding--;
      }
      abAppend(ab, welcome, welcomelen);
    } else {
      abAppend(ab, "*", 1);
    }
    abAppend(ab, "\x1b[K", 3);

    if (i != E.screenrows - 1)
      abAppend(ab, "\r\n", 2);
  }
}

void editorRefreshScreen() {
  struct abuf ab = ABUF_INIT;
  abAppend(&ab, "\x1b[?25l", 6); // hides the cursor
  abAppend(&ab, "\x1bH", 3);     // moves the cursor to top left`
  editorDrawRows(&ab);           // draws stars
  abAppend(&ab, "\x1bH", 3);     // mpves cursor to top left
  abAppend(&ab, "\x1b[?25h", 6); // brings back the cursor
  write(STDOUT_FILENO, ab.b, ab.len);
  abFree(&ab);
}

/*
   intialization section
 */

void initEditor() { // initialize the struct that stores all the values of the
                    // terminal;
  if (getWindowSize(&E.screenrows, &E.screencols) == -1)
    die("getWindowSize");
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
