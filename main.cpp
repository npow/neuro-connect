#include <cassert>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <random>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>
#include <State.h>

#include "doublefann.h"
#include "fann_cpp.h"

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
      return currState.getCurrTurn();
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

    const State& getCurrState() const {
      return currState;
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
          goodness = evaluate(currState, currTurn, maxDepth, -numeric_limits<int>::max(), -bestWorst);
          cout << move.toString() << ", goodness: " << goodness << endl;
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

    int evaluate(State& s, const Player player, const int currDepth, int alpha, int beta) {
      const int alphaOrig = alpha;
      const Hash_t hash = s.getHash(player);
      const auto& it = stateMap.find(hash);
      if (it != stateMap.end()) {
        if (abs(it->second.bestValue) > 100000) {
          return it->second.bestValue;
        } else if (it->second.depth >= currDepth) {
          if (it->second.flag == Flag::EXACT) {
            return it->second.bestValue;
          } else if (it->second.flag == Flag::LOWERBOUND) {
            alpha = max(alpha, it->second.bestValue);
          } else if (it->second.flag == Flag::UPPERBOUND) {
            beta = min(beta, it->second.bestValue);
          }
          if (alpha > beta) {
            return it->second.bestValue;
          }
        }
      }
      if (s.hasPlayerWon(player)) {
        return numeric_limits<int>::max() + currDepth - maxDepth;
      }
      else if (s.hasPlayerWon(OTHER(player))) {
        return -(numeric_limits<int>::max() + currDepth - maxDepth);
      } else if (checkIsGameDrawn(s)) {
        return 0;
      } else if (currDepth == 0) {
        return s.getGoodness(player);
      }
      else {
        // now it's the other player's turn
        int bestVal = -numeric_limits<int>::max();
        int maxab = alpha;
        vector<Move> moves = s.getMoves(OTHER(player));
        for (auto& move : moves) {
          pushState(s);

          assert(s.move(move.x, move.y, move.dir, true));
          int goodness = evaluate(s, OTHER(player), currDepth-1, -beta, -alpha);
          if (goodness > bestVal) {
            bestVal = goodness;
            if (bestVal > maxab) {
              maxab = bestVal;
            }
          }

          s = popState();

#if USE_AB_PRUNING
          if (bestVal > beta) {
            break;
          }
#endif
        }

        Data d;
        d.bestValue = -bestVal;
        d.depth = currDepth;
        if (d.bestValue <= alphaOrig) {
          d.flag = Flag::UPPERBOUND;
        } else if (d.bestValue >= beta) {
          d.flag = Flag::LOWERBOUND;
        } else {
          d.flag = Flag::EXACT;
        }
        stateMap[hash] = d;

        return -bestVal;
      }
    }

    void setStateMap(const StateMap_t& stateMap) {
      this->stateMap = stateMap;
    }

    const StateMap_t& getStateMap() const {
      return stateMap;
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
    State currState;
    Player currTurn;
    vector<State> history;
    StateMap_t stateMap;
};

static void dumpStateMap(const int width, const int height, const StateMap_t& stateMap, const string& fileName) {
  ofstream out(fileName.c_str());
  for (const auto& p : stateMap) {
    out << p.first.to_ulong() << " " << p.second.depth << " " << p.second.bestValue
        << " " << static_cast<int>(p.second.flag) << endl;
  }
  out.close();
}

static StateMap_t loadStateMap(const std::string& fileName) {
  cout << "Loading statemap: " << fileName << endl;
  StateMap_t stateMap;
  ifstream in(fileName);
  string line;
  while (getline(in, line)) {
    unsigned long h = 0;
    int goodness = 0;
    int flag;
    Data d;
    stringstream ss(line);
    ss >> h >> d.depth >> d.bestValue >> flag;
    Hash_t hash(h);
    d.flag = static_cast<Flag>(flag);
    if (h) {
      stateMap[hash] = d;
    }
  }
  in.close();

  cout << "Done loading statemap: " << stateMap.size() << endl;
  return stateMap;
}

inline bool fileExists(const std::string& name) {
  struct stat buffer;
  return (stat(name.c_str(), &buffer) == 0);
}

static int countDraws(const StateMap_t& stateMap) {
  int numStates = 0;
  for (const auto& p : stateMap) {
    if (abs(p.second.bestValue) > 100000) { // either a win or loss
      continue;
    }
    numStates++;
  }
  return numStates;
}

static void populateStates(const int width, const int height, const int maxDepth, const std::string& fileName) {
  unsigned long h = 0;
  int numStates = 0;
  Game game(width, height, maxDepth);
  StateMap_t savedStateMap = loadStateMap(fileName+"_statemap");
  game.setStateMap(savedStateMap);
  ifstream in(fileName.c_str());
  while (in >> h) {
    State s(width, height);
    Hash_t hash(h);
    s.fromHash(hash);

    game.setCurrState(s);
    game.evaluate(s, Player::WHITE, maxDepth, -numeric_limits<int>::max(), numeric_limits<int>::max());

#if 1
    numStates++;
    if (numStates % 500 == 0) {
      cout << numStates << endl;
    }
#endif
  }

  const StateMap_t& stateMap = game.getStateMap();
  cout << "After: " << countDraws(stateMap) << endl;
  dumpStateMap(width, height, stateMap, fileName+"_statemap");
}

static void generateStates(const int width, const int height) {
  const int n = width * height;
  const int r = 8;
  vector<bool> v(n);
  fill(v.begin() + n - r, v.end(), true);

  unordered_set<Hash_t> states;
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

      State s1(width, height);
      s1.fromHash(s.getHash(Player::WHITE));
      assert(s == s1);

      states.insert(s.getHash(Player::WHITE));
    }
  } while (next_permutation(v.begin(), v.end()));
  stringstream ss;
  ss << "states_" << width << "_" << height << ".txt";
  ofstream out(ss.str());
  for (const auto& s : states) {
    out << s.to_ulong() << endl;
  }
  out.close();
}

void createTrainData(const int width, const int height, const std::string& fileName) {
  ofstream out("train.dat");
  StateMap_t savedStateMap = loadStateMap(fileName);
#if 0
  out << "y";
  for (int i = 0; i < 20; ++i) {
    out << ",x" << (i+1);
  }
#endif
  out << savedStateMap.size() << " 40 3" << endl;
  for (const auto& d : savedStateMap) {
    int board[4][5] = {
      { 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0 },
      { 0, 0, 0, 0, 0 }
    };
    State s(width, height);
    s.fromHash(d.first);

    for (const auto& p : s.getPieces(Player::WHITE)) {
      const int x = p.x - 1;
      const int y = p.y - 1;
      board[y][x] = 1;
    }

    for (const auto& p : s.getPieces(Player::BLACK)) {
      const int x = p.x - 1;
      const int y = p.y - 1;
      board[y][x] = 2;
    }

    int numPieces = 0;
    for (int i = 0; i < height; ++i) {
      for (int j = 0; j < width; ++j) {
        if (board[i][j] == 1 || board[i][j] == 2) {
          numPieces++;
        }
        out << (board[i][j] == 1 ? "0 1" : (board[i][j] == 2 ? "1 0" : "0 0")) << (i == (height-1) && j == (width-1) ? "" : " ");
      }
    }
    assert(numPieces == NUM_PIECES_PER_SIDE*2);
    out << endl;
    out << (d.second.bestValue > 100000 ? "1 0 0" : (d.second.bestValue < -100000 ? "0 1 0" : "0 0 1")) << endl;
  }
  out.close();
}

static int print_callback(FANN::neural_net &net, FANN::training_data &train,
    unsigned int max_epochs, unsigned int epochs_between_reports,
    float desired_error, unsigned int epochs, void *user_data)
{
    cout << "Epochs     " << setw(8) << epochs << ". "
         << "Current Error: " << left << net.get_MSE() << right << endl;
    return 0;
}

void trainNeuralNet(const std::string& fileName) {
  const float learning_rate = 0.01f;
  const float learning_momentum = 0.9f;
  const unsigned int layers[6] = { 40, 80, 40, 20, 10, 3 };
  const float desired_error = 0.0000001f;
  const unsigned int max_iterations = 5;
  const unsigned int iterations_between_reports = 1;

  cout << endl << "Creating network." << endl;

  FANN::neural_net net;
  net.create_standard_array(6, layers);
  net.set_training_algorithm(FANN::TRAIN_BATCH);
  net.set_learning_rate(learning_rate);
  net.set_learning_momentum(learning_momentum);
  net.set_activation_function_hidden(FANN::SIGMOID_SYMMETRIC_STEPWISE);
  net.set_activation_function_output(FANN::SIGMOID_SYMMETRIC_STEPWISE);

  cout << endl << "Network Type                         :  ";
  switch (net.get_network_type())
  {
    case FANN::LAYER:
      cout << "LAYER" << endl;
      break;
    case FANN::SHORTCUT:
      cout << "SHORTCUT" << endl;
      break;
    default:
      cout << "UNKNOWN" << endl;
      break;
  }
  net.print_parameters();

  FANN::training_data data;
  if (data.read_train_from_file(fileName)) {
    net.init_weights(data);

    cout << "Max Epochs " << setw(8) << max_iterations << ". "
      << "Desired Error: " << left << desired_error << right << endl;
    net.set_callback(print_callback, NULL);
    net.train_on_data(data, max_iterations, iterations_between_reports, desired_error);

    net.save("neuroconnect_5_4.net");
  }
}

void runTests(const int width, const int height, const std::string& fileName) {
  createTrainData(width, height, fileName);
  //trainNeuralNet(fileName);
}

int main(int argc, char* const argv[]) {
  bool isAuto = false;
  bool isWhite = true;
  bool isSmallBoard = true;
  bool isGenMode = false;
  bool isPopMode = false;
  bool isTestMode = false;
  int maxDepth = 5;
  string stateMapFileName;
  char c = '\0';
  while ((c = getopt(argc, argv, "abd:glhp:t:")) != -1) {
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
      case 't':
        isTestMode = true;
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
  } else if (isTestMode) {
    runTests(width, height, stateMapFileName);
    return 0;
  }

  Game game(width, height, maxDepth);
  const Player player = isWhite ? Player::WHITE : Player::BLACK;
  while (game.getWinner() == Player::NONE) {
    cout << endl << endl << "turn#: " << game.getNumTurns() << (game.getCurrTurn() == Player::WHITE ? " (W)" : " (B)") << endl;
    game.getCurrState().print();
    Timer t;
    if (game.getCurrTurn() == player) {
      shared_ptr<Move> bestMove = game.getBestMove();
      string res = "N/A";
      if (bestMove) {
        res = bestMove->toString();
        cout << res << endl;
        game.move(res, true);
      }
    } else {
      if (isAuto) {
        shared_ptr<Move> bestMove = game.getBestMove();
        string res = "N/A";
        if (bestMove) {
          res = bestMove->toString();
          cout << res << endl;
          game.move(res, true);
        }
      } else {
        string cmd;
        cin >> cmd;
        cout << "Got: " << cmd << endl;
        if (!game.move(cmd, false)) {
          cout << "Invalid move: " << cmd << endl;
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
