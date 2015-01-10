#include <unistd.h>
#include <State.h>

int main(int argc, char* const argv[]) {
  bool isWhite = true;
  bool isSmallBoard = true;
  char c = '\0';
  while ((c = getopt(argc, argv, "bl")) != -1) {
    switch (c) {
      case 'b':
        isWhite = false;
        break;
      case 'l':
        isSmallBoard = false;
        break;
    }
  }
  cout << "isWhite: " << isWhite << endl;
  cout << "isSmallBoard: " << isSmallBoard << endl;

  int width = 5;
  int height = 4;
  if (!isSmallBoard) {
    width = 7;
    height = 6;
  }

  State s(width, height);
  s.print();
  return 0;
}
