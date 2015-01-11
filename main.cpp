#include <cassert>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <sys/stat.h>
#include <unistd.h>
#include <State.h>
using namespace std;

#define USE_AB_PRUNING 1

class Game {
  public:
    Game(const int width, const int height, const int maxDepth) : numTurns(0), maxDepth(maxDepth), currTurn(Player::WHITE), currState(State(width, height)) {
    }

    bool move(const std::string& move, bool skipValidation = false) {
      if (move.length() != 3) {
        return false;
      }
      const int x = toInt(move[0]);
      const int y = toInt(move[1]);
      Direction dir = Direction::END;
      switch (move[2]) {
        case 'N': dir = Direction::N; break;
        case 'S': dir = Direction::S; break;
        case 'E': dir = Direction::E; break;
        case 'W': dir = Direction::W; break;
      }
      if (dir == Direction::END) {
        return false;
      }
      bool retval = currState.move(x, y, dir, false);
      if (retval) {
        numTurns++;
        currTurn = OTHER(currTurn);
        currState.print();
        history.push_back(currState);
      }
      return retval;
    }

    Player getCurrTurn() const {
      return currTurn;
    }

    Player getWinner() const {
      return currState.getWinner();
    }

    int getNumTurns() const {
      return numTurns;
    }

    void setCurrState(const State& state) {
      currState = state;
    }

    bool checkIsGameDrawn(const State& s) const {
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

    bool isDraw() const {
      return checkIsGameDrawn(currState);
    }

    shared_ptr<Move> getBestMove() {
      vector<Move> moves = currState.getMoves(currTurn);
      shared_ptr<Move> bestMove;
      int goodness = 0;
      int bestWorst = -numeric_limits<int>::max();
      for (auto& move : moves) {
        pushState(currState);

        assert(currState.move(move.x, move.y, move.dir, true));
        if (currState.hasPlayerWon(currTurn)) {
          bestMove = make_shared<Move>(move);
          bestWorst = numeric_limits<int>::max();
          currState = popState();
          break;
        } else {
          goodness = evaluate(currState, currTurn, 0, -numeric_limits<int>::max(), -bestWorst);
          if (goodness > bestWorst) {
            bestWorst = goodness;
            bestMove = make_shared<Move>(move);
          } else if (goodness == bestWorst) {
            if (find(history.begin(), history.end(), currState) == history.end() || rand() % 2 == 0) {
              bestMove = make_shared<Move>(move);
            }
          }
        }

        currState = popState();
      }
      cout << "bestWorst: " << bestWorst << endl;

      return bestMove;
    }

    int evaluate(State& s, const Player player, const int currDepth, int alpha, const int beta) {
      if (s.hasPlayerWon(player)) {
        return numeric_limits<int>::max() - currDepth;
      }
      else if (s.hasPlayerWon(OTHER(player))) {
        return -(numeric_limits<int>::max() - currDepth);
      } else if (checkIsGameDrawn(s)) {
        return 0;
      } else if (currDepth == maxDepth) {
        return s.getGoodness(player);
      }
      else {
        // now it's the other player's turn
        int bestVal = -numeric_limits<int>::max();
        vector<Move> moves = s.getMoves(OTHER(player));
        for (auto& move : moves) {
          pushState(s);

          assert(s.move(move.x, move.y, move.dir, true));
          int val = evaluate(s, OTHER(player), currDepth+1, -beta, -alpha);
          bestVal = max(bestVal, val);
          alpha = max(alpha, val);

          s = popState();

#if USE_AB_PRUNING
          if (alpha > beta) {
            break;
          }
#endif
        }

        return -bestVal;
      }
    }

  private:
    void pushState(const State& s) {
      history.push_back(s);
    }

    State popState() {
      State s = history.back();
      history.pop_back();
      return s;
    }

  private:
    int numTurns;
    int maxDepth;
    Player currTurn;
    State currState;
    vector<State> history;
};

static void dumpStateMap(const int width, const int height, const map<string, int>& stateMap, const string& fileName) {
  stringstream ss;
  ss << fileName << "_statemap";

  ofstream out(ss.str());
  for (const auto& p : stateMap) {
    out << p.first << " " << p.second << endl;
  }
  out.close();
}

#if 0
static map<string, int> loadStateMap(const int width, const int height) {
  stringstream ss;
  ss << "stateMap_" << width << "_" << height << ".txt";

  map<string, int> stateMap;
  ifstream in(ss.str());
  while (!in.eof()) {
    string stateStr;
    int goodness;
    in >> stateStr >> goodness;
    stateMap[stateStr] = goodness;
  }
  in.close();

  return stateMap;
}
#endif

inline bool fileExists(const std::string& name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

static void populateStates(const int width, const int height, const int maxDepth, const std::string& fileName) {
  cout << fileName << endl;
  if (!fileExists(fileName)) {
    cout << "File not found: " << fileName << endl;
    return;
  }

  ifstream in(fileName.c_str());
  string line;
  vector<State> states;
  states.reserve(8817900);
  while (in >> line) {
    State s(width, height);
    s.fromString(line);
    states.push_back(s);
  }
  in.close();

  map<string, int> stateMap;
  for (auto& s : states) {
    Game game(width, height, maxDepth);
    game.setCurrState(s);
    stateMap[s.toString(Player::WHITE)] = game.evaluate(s, Player::WHITE, 0, -numeric_limits<int>::max(), numeric_limits<int>::max());
  }

  dumpStateMap(width, height, stateMap, fileName);
}

static void generateStates(const int width, const int height) {
  const int n = width * height;
  const int r = 8;
  vector<bool> v(n);
  fill(v.begin() + n - r, v.end(), true);

  vector<string> states;
  states.reserve(8817900);
  do {
    vector<Piece> pieces;
    pieces.reserve(r);
    for (int i = 0; i < n; ++i) {
      if (v[i]) {
        const int x = (i % width) + 1;
        const int y = (i / width) + 1;
        pieces.push_back(Piece(x, y));
      }
    }

    const vector< vector<int> >& ps = getCombinations_8_4();
    for (const auto& p : ps) {
      vector<Piece> whitePieces;
      vector<Piece> blackPieces;
      for (const auto& i : p) {
        whitePieces.push_back(pieces[i]);
      }
      for (int i = 0; i < r; ++i) {
        if (find(p.begin(), p.end(), i) == p.end()) {
          blackPieces.push_back(pieces[i]);
        }
      }

      State s(width, height);
      s.setPieces(whitePieces, blackPieces);
      states.push_back(s.toString(Player::WHITE));
    }
  } while (next_permutation(v.begin(), v.end()));
  stringstream ss;
  ss << "states_" << width << "_" << height << ".txt";
  ofstream out(ss.str());
  for (const auto& s : states) {
    out << s << endl;
  }
  out.close();
}

int main(int argc, char* const argv[]) {
  bool isAuto = false;
  bool isWhite = true;
  bool isSmallBoard = true;
  bool isGenMode = false;
  bool isPopMode = false;
  int maxDepth = 10;
  string stateMapFileName;
  char c = '\0';
  while ((c = getopt(argc, argv, "abd:glhp:")) != -1) {
    switch (c) {
      case 'a':
        isAuto = true;
        break;
      case 'b':
        isWhite = false;
        break;
      case 'd':
        maxDepth = atoi(optarg);
        break;
      case 'g':
        isGenMode = true;
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
      case 'p':
        isPopMode = true;
        stateMapFileName = optarg;
        break;
    }
  }
  cout << "isWhite: " << isWhite << endl;
  cout << "isSmallBoard: " << isSmallBoard << endl;
  cout << "maxDepth: " << maxDepth << endl;

  int width = 5;
  int height = 4;
  if (!isSmallBoard) {
    width = 7;
    height = 6;
  }

  if (isGenMode) {
    generateStates(width, height);
    return 0;
  } else if (isPopMode) {
    populateStates(width, height, maxDepth, stateMapFileName);
    return 0;
  }

  Game game(width, height, maxDepth);
  const Player player = isWhite ? Player::WHITE : Player::BLACK;
  while (game.getWinner() == Player::NONE) {
    cout << endl << endl << "turn#: " << game.getNumTurns() << (game.getCurrTurn() == Player::WHITE ? " (W)" : " (B)") << endl;
    Timer t;
    if (game.getCurrTurn() == player) {
      shared_ptr<Move> bestMove = game.getBestMove();
      string res = "N/A";
      if (bestMove) {
        res = bestMove->toString();
        game.move(res, true);
      }
      cout << res << endl;
    } else {
      if (isAuto) {
        shared_ptr<Move> bestMove = game.getBestMove();
        string res = "N/A";
        if (bestMove) {
          res = bestMove->toString();
          game.move(res, true);
        }
        cout << res << endl;
      } else {
        string cmd;
        cin >> cmd;
        cout << "Got: " << cmd << endl;
        if (!game.move(cmd, false)) {
          cout << "Invalid move: " << cmd << endl;
          break;
        }
      }
    }
    if (game.isDraw()) {
      cout << "Draw by 3-fold repetition" << endl;
      break;
    }
  }

  return 0;
}
