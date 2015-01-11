#include <cassert>
#include <limits>
#include <memory>
#include <deque>
#include <unistd.h>
#include <State.h>
using namespace std;

#define USE_AB_PRUNING 1
#define MAX_DEPTH 8

static deque<State> history;
static int getCurrDepth() {
  return history.size();
}

static void pushState(const State& s) {
  history.push_back(s);
}

static State popState() {
  State s = history.back();
  history.pop_back();
  return s;
}

static bool isDraw(const State& s) {
  int i = 0;
  int numRepeat[2] = { 0, 0 };
  for (const auto& state : history) {
    if (s == state) {
      numRepeat[i % 2]++;
      if (numRepeat[0] == 3 || numRepeat[1] == 3) {
        return true;
      }
    }
    i++;
  }
  return false;
}

static int evaluate(State& s, const Player player, const int maxDepth, const int alpha, const int beta) {
  if (s.hasPlayerWon(player)) {
    return numeric_limits<int>::max() - getCurrDepth();
  }
  else if (s.hasPlayerWon(OTHER(player))) {
    return -(numeric_limits<int>::max() - getCurrDepth());
  } else if (isDraw(s)) {
    return 0;
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

      assert(s.move(move.x, move.y, move.dir, true));
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
  shared_ptr<Move> bestMove;
  int goodness = 0;
  int bestWorst = -numeric_limits<int>::max();
  for (auto& move : moves) {
    pushState(s);

    assert(s.move(move.x, move.y, move.dir, true));
    if (s.hasPlayerWon(player)) {
      bestMove = make_shared<Move>(move);
      s = popState();
      break;
    } else {
      goodness = evaluate(s, player, maxDepth, -numeric_limits<int>::max(), -bestWorst);
      if (goodness > bestWorst) {
        bestWorst = goodness;
        bestMove = make_shared<Move>(move);
      } else if (goodness == bestWorst) {
        if (find(history.begin(), history.end(), s) == history.end() || rand() % 2 == 0) {
          bestMove = make_shared<Move>(move);
        }
      }
    }

    s = popState();
  }

  if (bestMove) {
    s.move(bestMove->x, bestMove->y, bestMove->dir, true);
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
    Timer t;
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
    if (isDraw(s)) {
      cout << "Draw by 3-fold repetition" << endl;
      break;
    }
    currTurn = OTHER(currTurn);
  }
  return 0;
}
