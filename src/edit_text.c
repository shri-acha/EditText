/*includes*/
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <termios.h>
/*declarations*/

struct termios orig_mode;

/*terminal*/

void die(const char* err_msg){
  perror(err_msg);
  exit(1);
}

void disableRawMode(){
  if (tcsetattr(STDIN_FILENO,TCSAFLUSH,&orig_mode)==-1)die("[tcsetattr]"); // TCSFLUSH makes it so that the i/o buffer flushes 
}


void EnableRawMode(){ 
  struct termios raw = orig_mode;
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


int main(int argc, char** argv){
  EnableRawMode(); //Enables raw mode
  char test_char= '\0';
  while( 1 ){
    if (read(STDIN_FILENO,&test_char,1) == -1 && errno !=EAGAIN )die("[read]");

    if (iscntrl(test_char)){
      printf("\r%d\n",test_char);
    }
    else {
      printf("\r%d ('%c')\n",test_char,test_char);
    }
    if(test_char =='q'){
      break;
    }
  }
  return 0;
}
