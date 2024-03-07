#include "stdio.h"
int main(int argc, char** argv) {
  printf("prog_no_arg from disk\n");
  printf("argc:%d ,argv[0]:%s ,argv[1]:%s\n", argc, argv[0], argv[1]);
  return 0;
}