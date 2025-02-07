/*includes*/
#include <asm-generic/ioctls.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
/*declarations*/
// MASKS the last 5 bits of a character. 
#define CTRL_KEY(k) ((k)&0x1f) 
struct editor_config{
  int cx,cy;
  int screen_rows;
  int screen_cols;
  struct termios orig_mode;
};
  struct editor_config E;
/*terminal*/

void die(const char* err_msg){
  write(STDOUT_FILENO,"\x1b[2J",4); //ESCAPE SEQ: \x1b[
  write(STDOUT_FILENO,"\x1b[H",3);
  perror(err_msg);
  exit(1);
}

void disableRawMode(){
  if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&E.orig_mode)==-1)die("[tcsetattr]"); // TCSAFLUSH makes it so that the i/o buffer flushes 
}


void EnableRawMode(){ 
  struct termios raw = E.orig_mode;
  tcgetattr(STDIN_FILENO,&raw);
  atexit(disableRawMode);

  raw.c_lflag &= ~( ISTRIP | ECHO | IEXTEN |  ICANON | ISIG);  
  raw.c_oflag &= ~( OPOST );
  raw.c_iflag &= ~( INPCK | BRKINT | ICRNL | IXON  );  
  raw.c_cflag |= CS8;
  raw.c_cc[VMIN] = 0; // A minimum of 0 bytes need to be serial input for the read() to return 
  raw.c_cc[VTIME] = 1; // A minimum of 1 * 1/10th of a second is needed for the the read() to return 
  if(tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw) == -1)die("[tcsetattr]");
}

char editor_read_keypress(){
  int rread;
  char c;
  while((rread = read(STDIN_FILENO,&c,1)) != 1){ //aparently assignment returns 1 on error 
    if(rread == -1 && errno != EAGAIN) die("[read]");
  }
  if (c=='\x1b'){
    char seq[3];

    if( read(STDIN_FILENO,&seq[0],1) != 1)return '\x1b';
    if( read(STDIN_FILENO,&seq[1],1) != 1)return '\x1b';

    if (seq[0]=='['){
      switch(seq[1]){
        case 'A': return 'h';
        case 'B': return 'l';
        case 'C': return 'j';
        case 'D': return 'k';
      }
    }
  }

  return c;
}

int cursor_get_position(int* rows,int* cols){
 
  char buf[32];
  unsigned int i = 0;

  if (write(STDOUT_FILENO,"\x1b[6n",4)==-1)return -1; 
  while (i < sizeof(buf)){
     if(read(STDIN_FILENO,&buf[i],1)!=1) break;
     if( buf[i]=='R')break;  // just before R we get the escape sequence.
    i++;
  }

  buf[i] = '\0';
  if (buf[0]!= '\x1b' || buf[1]!='[') return -1;
  if (sscanf(&buf[2],"%d;%d",rows,cols)!=2) return -1;
  

  editor_read_keypress();
  return -1;
}


int get_window_size(int* rows, int* cols){
  struct winsize ws;
  
  if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws) == -1 || ws.ws_col == 0){
    if ( write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12)return -1;
    return cursor_get_position(rows,cols);
  }
  else {
    *cols = ws.ws_col;
    *rows = ws.ws_row;
    return 0;
  }
}

/*append buffer*/

struct abuf {
  char *b;
  int len;
}; //just like how go does it.
#define ABUF_INIT {NULL,0}
void ab_append(struct abuf *ab ,const char* s,int len){
  char* new = realloc(ab->b,ab->len+len);
  if (new == NULL){
    return;
  }
  memcpy(&new[ab->len],s,len); // copies s into *new after the initial length of the string
  ab->b = new;
  ab->len += len;
  
}

void ab_free(struct abuf *ab){
  free(ab->b);
}

/* input */

void editor_move_cursor(char key){
  switch (key){
  case 'h':
    E.cx--; 
  break;
  case 'l':
    E.cx++; 
  break;
  case 'j':
    E.cy++; 
  break;
  case 'k':
    E.cy--; 
  break;
  }
}

void editor_process_keypress(){
  char kp = editor_read_keypress();

  switch(kp){
    case CTRL_KEY('q'):
      write(STDOUT_FILENO,"\x1b[2J",4); //ESCAPE SEQ: \x1b[
      write(STDOUT_FILENO,"\x1b[H",3);
      exit(0);
      break;
    case 'h':
    case 'j':
    case 'k':
    case 'l':
      editor_move_cursor(kp);
    break;
  }
}



void init_editor(){
  E.cx = E.cy = 0;
  if(get_window_size(&E.screen_rows, &E.screen_cols) == -1){
      die("[get_window_size]");
  }
}

void editor_draw_rows(struct abuf* ab){
  for(int y=0;y<E.screen_rows;y++){
    ab_append(ab,"~",1);
    if( y<E.screen_rows-1)ab_append(ab, "\r\n",2);
  }
}

void editor_refresh_screen(){
  struct abuf ab = ABUF_INIT;
  
  ab_append(&ab,"\x1b[?25l",6); //shows the cursor
  ab_append(&ab,"\x1b[K",3);
  ab_append(&ab,"\x1b[H",3); 


  editor_draw_rows(&ab);

  char buf[32];
  /*ab_append(&ab,"\x1b[2J",3);*/
  snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy + 1 , E.cx + 1);
  ab_append(&ab,buf,strlen(buf));

  ab_append(&ab,"\x1b[?25h",6); //hides the cursor
  write(STDOUT_FILENO,ab.b,ab.len);

  ab_free(&ab);
}

/* init */
int main(int argc, char** argv){
  EnableRawMode();  
  init_editor();
  while( 1 ){
    editor_refresh_screen();
    editor_process_keypress(); // process blocks around here
  }
  return 0;
}
