#include <climits>
#include <unistd.h>
#include <State.h>

#define OTHER(player) ((player) == Player::WHITE ? Player::BLACK : Player::WHITE)
#define MAX_DEPTH 20

static void pushState() {}
static void popState() {}

static int evaluate(State& s, Player player, int level, int alpha, int beta) {
  const Player winner = s.getWinner();
  if (winner == player) {
    return INT_MAX - MAX_DEPTH;
  }
  else if (winner == OTHER(player)) {
    return -(INT_MAX - MAX_DEPTH);
  }
  else if (level == MAX_DEPTH) {
    return s.getGoodness(player);
  }
  else {
    int best = -(INT_MAX);
    int maxab = alpha;
    vector<Move> moves = s.getMoves(player);
    for (vector<Move>::iterator it = moves.begin(); it != moves.end(); ++it) {
      pushState();

      s.movePiece(it->piece, it->dir);
      int goodness = evaluate(s, OTHER(player), level, -beta, -maxab);
      if (goodness > best) {
        best = goodness;
        if (best > maxab) {
          maxab = best;
        }
      }

      popState();

      if (best > beta) {
        break;
      }
    }

    return -best;
  }
}

static string makeMove(State& s, const Player player, int level) {
  vector<Move> moves = s.getMoves(player);
  Move* bestMove = NULL;
  int goodness = 0;
  int bestWorst = -INT_MAX;
  for (vector<Move>::iterator it = moves.begin(); it != moves.end(); ++it) {
    pushState();

    s.movePiece(it->piece, it->dir);
    if (s.getWinner() == player) {
      bestMove = &*it;
      popState();
      break;
    } else {
      goodness = evaluate(s, player, level, -INT_MAX, -bestWorst);
      if (goodness > bestWorst) {
        bestWorst = goodness;
        bestMove = &*it;
      } else if (goodness == bestWorst) {
        if (rand() % 2 == 0) {
          bestMove = &*it;
        }
      }
    }

    popState();
  }

  return bestMove ? bestMove->toString() : "";
}

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
