#include "print.h"

void main(void){
  unsigned int i = 0;
  while(i < 24){
    put_char('\n');
    put_int(i);
    i++;
  }

  put_str("\nWelcome to Chaos ..");
  put_str("\nchloe is a beauty in CityU!");
  while(1);
}
