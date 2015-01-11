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
#define MAX_DEPTH 8

static vector<State> history;
static int numTurns = 0;
static int getCurrDepth() {
  return history.size() - numTurns;
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
    int best = -numeric_limits<int>::max();
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
    cout << "bestWorst: " << bestWorst << endl;
    s.move(bestMove->x, bestMove->y, bestMove->dir, true);
    return bestMove->toString();
  }
  return "";
}

static void dumpStateMap(const int width, const int height, const map<string, int>& stateMap, const string& fileName) {
  stringstream ss;
  ss << fileName << "_statemap";

  ofstream out(ss.str());
  for (const auto& p : stateMap) {
    out << p.first << " " << p.second << endl;
  }
  out.close();
}

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

inline bool fileExists(const std::string& name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

static void populateStates(const int width, const int height, const std::string& fileName) {
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
    stateMap[s.toString()] = evaluate(s, Player::WHITE, MAX_DEPTH, -numeric_limits<int>::max(), numeric_limits<int>::max());
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

  State s(width, height);
  Player currTurn = Player::WHITE; // white starts
  const Player player = isWhite ? Player::WHITE : Player::BLACK;
  while (s.getWinner() == Player::NONE) {
    cout << endl << endl << "numTurns: " << numTurns << endl;
    history.push_back(s);
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
    numTurns++;
  }
  return 0;
}
