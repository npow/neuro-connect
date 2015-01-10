#include <cassert>
#include <limits>
#include <stack>
#include <unistd.h>
#include <State.h>
using namespace std;

#define USE_AB_PRUNING 0
#define MAX_DEPTH 5

static stack<State> history;
static int getCurrDepth() {
  return history.size();
}

static void pushState(const State& s) {
  history.push(s);
}

// FIXME: check whether we need to return the state or not
static State popState() {
  State s = history.top();
  history.pop();
  return s;
}

static int evaluate(State& s, const Player player, const int maxDepth, const int alpha, const int beta) {
  const Player winner = s.getWinner();
  if (winner == player) {
    return numeric_limits<int>::max() - getCurrDepth();
  }
  else if (winner == OTHER(player)) {
    return -(numeric_limits<int>::max() - getCurrDepth());
  }
  else if (maxDepth == getCurrDepth()) {
    return s.getGoodness(player);
  }
  else {
    // now it's the other player's turn
    int best = -(numeric_limits<int>::max());
    int maxab = alpha;
    vector<Move> moves = s.getMoves(OTHER(player));
    for (auto& move : moves) {
      pushState(s);

      assert(s.movePiece(move.piece, move.dir));
      int goodness = evaluate(s, OTHER(player), maxDepth, -beta, -maxab);
      if (goodness > best) {
        best = goodness;
        if (best > maxab) {
          maxab = best;
        }
      }

      s = popState();

#if USE_AB_PRUNING
      if (best > beta) {
        break;
      }
#endif
    }

    return -best;
  }
}

static string makeMove(State& s, const Player player, const int maxDepth) {
  vector<Move> moves = s.getMoves(player);
  Move* bestMove = NULL;
  int goodness = 0;
  int bestWorst = -numeric_limits<int>::max();
  for (auto& move : moves) {
    pushState(s);

    assert(s.movePiece(move.piece, move.dir));
    if (s.getWinner() == player) {
      bestMove = &move;
      s = popState();
      break;
    } else {
      goodness = evaluate(s, player, maxDepth, -numeric_limits<int>::max(), -bestWorst);
      if (goodness > bestWorst) {
        bestWorst = goodness;
        bestMove = &move;
      } else if (goodness == bestWorst) {
        if (rand() % 2 == 0) {
          bestMove = &move;
        }
      }
    }

    s = popState();
  }

  if (bestMove) {
    s.movePiece(bestMove->piece, bestMove->dir);
    return bestMove->toString();
  }
  return "";
}

int main(int argc, char* const argv[]) {
  bool isAuto = false;
  bool isWhite = true;
  bool isSmallBoard = true;
  char c = '\0';
  while ((c = getopt(argc, argv, "ablh")) != -1) {
    switch (c) {
      case 'a':
        isAuto = true;
        break;
      case 'b':
        isWhite = false;
        break;
      case 'l':
        isSmallBoard = false;
        break;
      case 'h':
        cout << "Usage: " << argv[0] << " [-a] [-b] [-l] [-h]" << endl
             << "\t-a\tAuto-mode. Play against itself." << endl
             << "\t-b\tPlay as black. Default is white. " << endl
             << "\t-l\tUse large board. Default is small board." << endl
             << "\t-h\tDisplay this help message." << endl;
        return 1;
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
  Player currTurn = Player::WHITE; // white starts
  const Player player = isWhite ? Player::WHITE : Player::BLACK;
  while (s.getWinner() == Player::NONE) {
    if (currTurn == player) {
      string res = makeMove(s, currTurn, MAX_DEPTH);
      cout << res << endl;
    } else {
      if (isAuto) {
        string res = makeMove(s, currTurn, MAX_DEPTH);
        cout << res << endl;
      } else {
        string cmd;
        cin >> cmd;
        cout << "Got: " << cmd << endl;
      }
    }
    s.print();
  }
  return 0;
}
