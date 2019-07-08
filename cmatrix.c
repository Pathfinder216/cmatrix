#include <sys/ioctl.h>
#include <sys/select.h>
#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>

//Escape character's value
#define ESC 27

//Max height and width allowed for cmatrix
#define MAX_WIDTH 300
#define MAX_HEIGHT 200

//Speed at which the characters fall
#define FALL_SPEED 6

//Console codes
#define TXT_RST "\x1B[0m"
#define HIDE_CURSOR "\e[?25l"
#define SHOW_CURSOR "\e[?25h"
#define CLEAR_SCRN "\033[H\033[J"

//List from which characters are randomly generated
#define CHARACTERS "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ!@#$%^&*()+=[]{};:/?.>,<|\\'\"`~"
#define CHAR_LIST_LENGTH 82

//Returns the min of two ints
#define MIN(a, b) ((a) > (b)) ? (b) : (a)

//Moves the cursor to specified position in the terminal
#define GO_TO_POS(row, col) printf("\033[%d;%dH", (row), (col))

//Array of color fade values, from darkest to brightest
char* GREENS[5] = {"\x1B[38;5;22m", "\x1B[38;5;28m", "\x1B[38;5;34m", "\x1B[38;5;40m", "\x1B[38;5;46m"};

struct matrixChar {
  char c;
  int isWhite;
  int runLength;
};

struct matrixChar matrix[MAX_HEIGHT][MAX_WIDTH];

static struct termios oldt;

//Restores terminal settings to the original settings
void restore_terminal_settings();

//Disables the terminal's default behavior of waiting
// for enter press to get input
void disable_waiting_for_enter();

//Shifts the matrix down one row and determines a new row
void updateMatrix();

//Determine the updated state of the space
void determineState(int row, int col);

//Determine the updated state of the top row in a column
void determineTopState(int col);

//Prints the entire matrix to the screen
void displayMatrix(int currWidth, int currHeight);

//Used to determine if the user pressed enter
int kbhit();

int main(int argc, char* argv[]) {
  int i, j;

  //The height and width used for printing to the screen
  int currWidth, currHeight;

  //Used to acquire terminal window size
  struct winsize w;

  disable_waiting_for_enter();

  printf(HIDE_CURSOR);

  //Creates empty space on terminal we can write to and initializes values in matrix
  for (i = 0; i < MAX_HEIGHT; i++) {
    //printf("\n");

    for (j = 0; j < MAX_WIDTH; j++) {
      matrix[i][j].c = ' ';
      matrix[i][j].isWhite = 0;
      matrix[i][j].runLength = 0;
    }
  }

  //Seed the random number generator
  srand(time(NULL));

  //Loop until the key pressed is ESC
  do {
    //Loop until the user presses a key
    do {
      //Retrieve current window size
      ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);

      //Number of columns/rows used is the number in the window, capped at the max
      currWidth = MIN(w.ws_col, MAX_WIDTH);
      currHeight = MIN(w.ws_row+1, MAX_HEIGHT);

      updateMatrix();
      displayMatrix(currWidth, currHeight);
 
      //Sleep for a fraction of a second
      usleep(625000 / FALL_SPEED);
    } while (!kbhit());
  } while (getchar() != ESC);

  printf(SHOW_CURSOR TXT_RST CLEAR_SCRN);

  return 0;
}

void updateMatrix() {
  int row, col;

  //Loop through all the columns
  for (col = MAX_WIDTH - 1; col >= 0; col--) {

    //Loop through all the rows, except the top row
    for (row = MAX_HEIGHT - 1; row > 0; row--) {

      //Determine the state of current space
      determineState(row, col);
    }

   //Determine the character on the top row
   determineTopState(col);
  }
}

void determineState(int row, int col) {
  //A run is occurring, so continue
  if (matrix[row][col].runLength > 0) {
    matrix[row][col].isWhite = 0;
    matrix[row][col].runLength--;
  }

  //A space "falls" into the current space
  else if (matrix[row-1][col].c == ' ') {
    matrix[row][col].c = ' ';
  }

  //A character "falls" into the current space
  else {
    matrix[row][col].c = CHARACTERS[rand() % CHAR_LIST_LENGTH];
    matrix[row][col].isWhite = matrix[row-1][col].isWhite;
    matrix[row][col].runLength = matrix[row-1][col].runLength;
  }
}

void determineTopState(int col) {
  //Continue a run
  if (matrix[0][col].runLength > 0) {
    matrix[0][col].runLength--;
    matrix[0][col].isWhite = 0;
  }
  //1 in 40 chance of starting a new character run
  else if (rand() % 40 < 1) {
    matrix[0][col].c = CHARACTERS[rand() % CHAR_LIST_LENGTH];
    
    //1 in 2 chance of starting character being white
    matrix[0][col].isWhite = (rand() % 2 < 1);

    //Run length is between 2 and 25, inclusive
    matrix[0][col].runLength = (rand() % 24) + 2;
  }
  //No run
  else {
    matrix[0][col].c = ' ';
    matrix[0][col].isWhite = 1;
  }
}

void displayMatrix(int currWidth, int currHeight) {
  int row, col;

  for (row = 0; row < currHeight; row++) {

    for (col = 0; col < currWidth; col++) {
      GO_TO_POS(row, col+1);
      
      if (matrix[row][col].isWhite) printf(TXT_RST);
      else printf(GREENS[MIN(matrix[row][col].runLength + 1, 4)]);
      
      printf("%c", matrix[row][col].c);
    }
  }
}

int kbhit() {
  struct timeval tv;
  fd_set fds;
  tv.tv_sec = 0;
  tv.tv_usec = 0;
  FD_ZERO(&fds);
  FD_SET(STDIN_FILENO, &fds); //STDIN_FILENO is 0
  select(STDIN_FILENO+1, &fds, NULL, NULL, &tv);
  return FD_ISSET(STDIN_FILENO, &fds);
}

void restore_terminal_settings() {
    tcsetattr(0, TCSANOW, &oldt);  /* Apply saved settings */
}

void disable_waiting_for_enter() {
    struct termios newt;

    /* Make terminal read 1 char at a time */
    tcgetattr(0, &oldt);  /* Save terminal settings */
    newt = oldt;  /* Init new settings */
    newt.c_lflag &= ~(ICANON | ECHO);  /* Change settings */
    tcsetattr(0, TCSANOW, &newt);  /* Apply settings */
    atexit(restore_terminal_settings); /* Make sure settings will be restored when program ends  */
}
