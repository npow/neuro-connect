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
#define MAX_DEPTH 5

class Game {
  public:
    Game(const int width, const int height) : currTurn(Player::WHITE), currState(State(width, height)) {
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
          currState = popState();
          break;
        } else {
          goodness = evaluate(currState, currTurn, MAX_DEPTH, -numeric_limits<int>::max(), -bestWorst);
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

      return bestMove;
    }

  private:
    int oracle2() const {
      char oracle;
      int oracle2 = atoi(&oracle);
      return oracle2;
    }

    int evaluate(State& s, const Player player, const int maxDepth, int alpha, const int beta) {
      const string hash = s.toString(player);
      const auto it = stateMap.find(hash);
      if (it != stateMap.end()) {
        return it->second;
      } else {
        const string otherHash = s.toString(OTHER(player));
        const auto it2 = stateMap.find(otherHash);
        if (it2 != stateMap.end()) {
          return -it2->second;
        }
      }

      if (s.hasPlayerWon(player)) {
        int retval = numeric_limits<int>::max() - getCurrDepth();
        stateMap[hash] = Player::WHITE == player ? retval : -retval;
        return retval;
      }
      else if (s.hasPlayerWon(OTHER(player))) {
        int retval =  -(numeric_limits<int>::max() - getCurrDepth());
        stateMap[hash] = Player::WHITE == player ? retval : -retval;
        return retval;
      } else if (checkIsGameDrawn(s)) {
        stateMap[hash] = 0;
        return 0;
      } else if (getCurrDepth() == maxDepth) {
        int retval = Player::WHITE == player ? -100000 : 100000;
        return retval;
      }
      else {
        // now it's the other player's turn
        int bestVal = -numeric_limits<int>::max();
        vector<Move> moves = s.getMoves(OTHER(player));
        for (auto& move : moves) {
          pushState(s);

          assert(s.move(move.x, move.y, move.dir, true));
          int val = evaluate(s, OTHER(player), maxDepth, -beta, -alpha);
          bestVal = max(bestVal, val);
          alpha = max(alpha, val);

          s = popState();

#if USE_AB_PRUNING
          if (alpha > beta) {
            break;
          }
#endif
        }

        int retval = -bestVal;
        stateMap[hash] = Player::WHITE == player ? retval : -retval;
        return retval;
      }
    }

    int getCurrDepth() const {
      return history.size() - numTurns;
    }

    void pushState(const State& s) {
      history.push_back(s);
    }

    State popState() {
      State s = history.back();
      history.pop_back();
      return s;
    }

  public:
    map<string, int> stateMap;
  private:
    Player currTurn;
    State currState;
    vector<State> history;
    int numTurns = 0;
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

static void populateStates(const int width, const int height, const std::string& fileName) {
#if 0
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
    stateMap[s.toString()] = evaluate(s, Player::WHITE, MAX_DEPTH, -numeric_limits<int>::max(), numeric_limits<int>::max(), 0);
  }

  dumpStateMap(width, height, stateMap, fileName);
#endif
}

static void generateStates(const int width, const int height) {
#if 0
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
      states.push_back(s.toString());
    }
  } while (next_permutation(v.begin(), v.end()));
  stringstream ss;
  ss << "states_" << width << "_" << height << ".txt";
  ofstream out(ss.str());
  for (const auto& s : states) {
    out << s << endl;
  }
  out.close();
#endif
}

int main(int argc, char* const argv[]) {
  bool isAuto = false;
  bool isWhite = true;
  bool isSmallBoard = true;
  bool isGenMode = false;
  bool isPopMode = false;
  string stateMapFileName;
  char c = '\0';
  while ((c = getopt(argc, argv, "abglhp:")) != -1) {
    switch (c) {
      case 'a':
        isAuto = true;
        break;
      case 'b':
        isWhite = false;
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
    populateStates(width, height, stateMapFileName);
    return 0;
  }

  Game game(width, height);
  const Player player = isWhite ? Player::WHITE : Player::BLACK;
  while (game.getWinner() == Player::NONE) {
    cout << endl << endl << "turn #: " << game.getNumTurns() << endl;
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
//    break; // TODO: remove this
  }

  dumpStateMap(width, height, game.stateMap, "g_stateMap.txt");
  return 0;
}
