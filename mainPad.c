#include <ncurses.h>
#include <stdlib.h>
#include "tinyfiledialogs.h"

#ifndef CTRL
#define CTRL(c) ((c) & 037)
#endif

#define ALT_ENTER 10
#define ALT_BACKSPACE 127
#define KEY_PLUS 43
#define KEY_DASH 45
#define BUFFER_LINES 1024

struct Cell {
  char data;
  struct Cell* next;
  struct Cell* prev;
};

enum globalStates {VISUAL = 2, INSERT = 1, NORMAL = 0} states;

struct Cell* head = NULL;
struct Cell* tail;
struct Cell* delete;
struct Cell* previous;
struct Cell* cursor;
struct Cell* copy_start;
struct Cell* copy_end;

WINDOW* pad;

int cursor_x=0;
int cursor_y=0;
int mypadpos = 0;
int mrow,mcols;

int copy_text[100];
char const * lFilterPatterns[2] = { "*.txt", "*.text" };
char const * lTheSaveFileName;
char const * lTheOpenFileName;

int letterCount()
{
  int count = 0;
  struct Cell* temp;
  temp = head;
  while (temp != NULL){
    count++;
    temp = temp -> next;
  }
  return count;
}

void cursorRefresh(int c)
{
  struct Cell* temp;
  int x=0,y=0;
  temp = head;
  while(temp != cursor){
    if(cursor -> data == ALT_ENTER && c == KEY_DOWN){y++;x=0;}
    if(cursor -> data == ALT_ENTER && c == KEY_UP){y--;x=0;}
    x++;
    temp = temp->next;
  }
  wmove(pad,y,x);
}

void insertBuffer(int c)
{
  struct Cell* insert = (struct Cell*) malloc(sizeof(struct Cell));
  insert -> data = c;
  insert -> next = NULL;
  insert -> prev = NULL;

  if(head == NULL)
  {
    head = insert;
    tail = insert;
    cursor = insert;
  }
  else if (cursor -> next == NULL && cursor == head){
    cursor -> next = insert;
    insert -> prev = cursor;
    cursor = insert;
    tail = insert;
  }
  else if (cursor -> next == NULL && cursor == tail){
    insert -> prev = cursor;
    cursor -> next = insert;
    cursor = insert;
    tail = insert;
  }
  else if (cursor -> next != NULL && cursor -> prev != NULL){
    insert -> next = cursor -> next;
    insert -> prev = cursor;
    cursor -> next -> prev = insert;
    cursor -> next = insert;
    cursor = insert;
  }
  else if (cursor -> next != NULL && cursor == head){
    if(cursor_x > 0){
      insert -> next = cursor -> next;
      insert -> prev = cursor;
      cursor -> next -> prev = insert;
      cursor -> next = insert;
      cursor = insert;
    }
    else{
      cursor -> prev = insert;
      insert -> next = cursor;
      cursor = insert;
      head = insert;
    }
  }
  getyx(pad,cursor_y,cursor_x);
  if(cursor_x == COLS-1 && cursor != tail){cursor_y++; cursor_x=0;}
  else if((int)insert ->data == ALT_ENTER){cursor_x=0; cursor_y++;}
  else {cursor_x++;}
  copy_start = NULL;
  copy_end = NULL;
}

void deleteFromBuffer()
{
  struct Cell* delete;
  delete = cursor;
  if(delete -> next == NULL && delete == head){
    free(cursor);
    free(delete);
    free(head);
    free(tail);
  }
  else if (delete -> next == NULL && delete == tail){
    cursor = cursor -> prev;
    tail = cursor;
    delete -> prev -> next = NULL;
    delete -> prev = NULL;
    free(delete);
  }
  else if (delete -> next != NULL && delete -> prev != NULL){
    cursor = cursor -> prev;
    cursor -> next = delete ->next;
    delete -> next -> prev = cursor;
    delete -> next = NULL;
    delete -> prev = NULL;
    free(delete);
  }
  else if (delete -> next != NULL && delete == head){
    if(cursor_x > 0){
      cursor = cursor -> next;
      head = cursor;
      cursor -> prev = NULL;
      delete -> next = NULL;
      free(delete);
    }
  }
  getyx(pad,cursor_y,cursor_x);
  if(cursor_x == 0 && cursor_y > 0){cursor_x = COLS-1; cursor_y--;}
  else if(cursor != head && letterCount() > 0){cursor_x--;}
  else{cursor_x--;}
}

void printBuffer()
{
  struct Cell* temp;
  temp = head;
  wclear(pad);
  while(temp != NULL){
    wprintw(pad,"%c",temp->data);
    temp = temp->next;
  }
}
void saveBufferToFile()
{
  lTheSaveFileName = tinyfd_saveFileDialog("SAVE FILE","save.txt",2,lFilterPatterns,NULL);
  if (! lTheSaveFileName)
    {
      tinyfd_messageBox(
                        "Error",
                        "Save file name is NULL",
                        "ok",
                        "error",
                        1);
    }
  struct Cell* save;
  save = head;
  FILE* saveFile;
  saveFile = fopen(lTheSaveFileName,"w");
  while(save != NULL){
    fprintf(saveFile, "%c", save -> data);
    save = save -> next;
  }
  wmove(pad,cursor_y+3,1);
  wprintw(pad,"!-- %s olarak kaydedildi. --! ",lTheSaveFileName);
  wmove(pad,cursor_y,cursor_x);
  fclose(saveFile);
}
void loadBufferFromFile()
{
  lTheOpenFileName = tinyfd_openFileDialog("let us read the password back","",2,lFilterPatterns,NULL,0);

	if (! lTheOpenFileName)
  {
      tinyfd_messageBox("Error", "Open file name is NULL", "ok", "error", 1);
  }
  FILE* loadFile;
  loadFile = fopen(lTheOpenFileName,"r");
  char c = getc(loadFile);
  while(c != EOF){
    insertBuffer(c);
    c = getc(loadFile);
  }
  fclose(loadFile);
}

void moveCursor(int c)
{
  getyx(pad,cursor_y,cursor_x);
  if(c == KEY_LEFT){
    if(cursor_x>0){
      cursor_x--;
      if(cursor -> prev != NULL){cursor = cursor -> prev;}
    }
  }
  if(c == KEY_SLEFT && states == VISUAL){
    if(cursor_x > 0){
      cursor_x--;
      wchgat(pad,1,A_REVERSE,1,NULL);
      if(cursor -> prev != NULL){
        if(copy_end == NULL){
          copy_end = cursor;
          cursor = cursor -> prev;
          copy_start = cursor;
        }
        else{
          cursor = cursor -> prev;
          copy_start = cursor;
        }
      }
    }
  }
  if(c == KEY_SRIGHT && states == VISUAL){
    if(cursor_x + 1 <= COLS && cursor != tail){
      cursor_x++;
      wchgat(pad,1,A_REVERSE,1,NULL);
      if(cursor -> prev != NULL){
        if(copy_start == NULL){
          copy_start = cursor;
          cursor = cursor -> next;
          copy_end = cursor;
        }
        else{
          cursor = cursor -> next;
          copy_end = cursor;
        }
      }
    }
  }
  if(c == KEY_RIGHT){
    if(cursor_x+1 <= COLS && cursor != tail){
      cursor_x++;
      if(cursor -> next != NULL){cursor = cursor -> next;}
    }
  }
  if(c == KEY_UP){
    if(cursor_y == 0 && mypadpos == 0){
      return;
    }
    else if(cursor_y == mypadpos && cursor_y >= 0 ){
      cursor_y--;
      mypadpos--;
      prefresh(pad, mypadpos, 0, 0, 0, LINES - 1, COLS - 1);
    }
    else {
      cursor_y--;
    };
    for(int i = 0; i < COLS; i++){
      if(cursor -> prev != NULL){
        if(cursor -> data == ALT_ENTER){
          cursor = cursor -> prev; break;
        }
        cursor = cursor -> prev;}
    }
  }
  if(c == KEY_DOWN){
    if( cursor_y == LINES - 1 && mypadpos == BUFFER_LINES - 1){
      return;
    }
    else if( mypadpos <= BUFFER_LINES+1 && cursor != tail){
      cursor_y++;
      mypadpos++;
      prefresh(pad, mypadpos, 0, 0, 0, LINES - 1, COLS - 1);
    }
    else {
      cursor_y++;
    }
    for(int i = 0; i < COLS; i++){
      if(cursor -> next != NULL){
        if(cursor -> data == ALT_ENTER){
          cursor = cursor -> prev; break;
        }
        cursor = cursor -> next;}
    }
  }
}
void copyToClipboard()
{
  int i = 0;
  struct Cell* copy;
  copy = copy_start;
  while(copy != copy_end){
    copy_text[i] = copy -> data;
    wmove(pad,cursor_y+3,i);
    wprintw(pad,"%c",copy_text[i]);
    copy = copy -> next;
    i++;
  }
  wmove(pad,cursor_y,cursor_x);
}
void pasteFromClipboard()
{
  int i=0;
  while(copy_text[i] != '\0'){
    insertBuffer((int)copy_text[i]);
    i++;
  }
  wmove(pad,cursor_y+3,1);
  wprintw(pad,"-- ! Yapistirma Basarili ! --");
  wmove(pad,cursor_y,cursor_x);
  while(copy_text[i] != '\0'){
    copy_text[i] = 0;
  }
}
void cutToClipboard()
{
  int i = 0;
  struct Cell* cut;
  cut = copy_start;
  while(cut != copy_end){
    copy_text[i] = cut -> data;
    wmove(pad,cursor_y+3,i);
    wprintw(pad,"%c",copy_text[i]);
    cut = cut -> next;
    deleteFromBuffer();
    i++;
  }
  wmove(pad,cursor_y,cursor_x);

}
void printStates()
{
  const char* state[3];
  state[0] = "- NORMAL -";
  state[1] = "- INSERT -";
  state[2] = "- VISUAL -";
  wmove(pad,LINES-2, 4);
  wprintw(pad,state[states]);
  wmove(pad,cursor_y,cursor_x);
}
int main()
{
  states = NORMAL;
  head = NULL;
  int input = 0;
  initscr();
  noecho();//disable auto-echoing
  cbreak();//making getch() work without a buffer I.E. raw characters
  keypad(stdscr,TRUE);//allows use of special keys, namely the arrow keys
  clear();    // empty the screen
  timeout(0);

  pad = newpad(BUFFER_LINES,COLS);
  keypad(pad,TRUE);

  getmaxyx(stdscr,mrow,mcols);
  prefresh(pad,0,0,0,0,LINES-1,COLS-1);
  //something to remember
  while( (input=wgetch(pad)) != 97)
  {
    if(input == KEY_LEFT || input == KEY_RIGHT || input == KEY_DOWN || input == KEY_UP || input == KEY_SLEFT || input == KEY_SRIGHT){
      moveCursor(input);
    }
    else if(input == ALT_BACKSPACE && states != VISUAL && states != NORMAL){
      deleteFromBuffer();
      printBuffer();
    }
    else if(input == CTRL('e')){saveBufferToFile();}
    else if(input == CTRL('r')){loadBufferFromFile();}
    else if(input == CTRL('d') && states == VISUAL){copyToClipboard();}
    else if(input == CTRL('f')){pasteFromClipboard();}
    else if(input == CTRL('x')){cutToClipboard();}
    else if(input == 118 && (states == NORMAL || states == INSERT)){states = VISUAL;}
    else if(input == CTRL('t') && (states == VISUAL || states == INSERT)){states = NORMAL;}
    else if(input == 105 && (states == NORMAL || states == VISUAL)){states = INSERT;}
    else if(states != VISUAL && states != NORMAL){
      insertBuffer(input);
      printBuffer();
    }
    wmove(pad,cursor_y,cursor_x);
    printStates();
    prefresh(pad,mypadpos,0,0,0,LINES-1,COLS-1);
  }
  delwin(pad);
  endwin();

  return 0;
}
