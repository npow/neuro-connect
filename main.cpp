#include <cassert>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <random>
#include <sys/stat.h>
#include <unistd.h>
#include <unordered_set>
#include "Game.h"
#include "State.h"
#include "tcpconnector.h"

using namespace std;

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

    int numExpanded = 0;
    game.setCurrState(s);
    game.evaluate(s, Player::WHITE, maxDepth, -numeric_limits<int>::max(), numeric_limits<int>::max(), numExpanded);

#ifndef NDEBUG
    numStates++;
    if (numStates % 1000 == 0) {
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
      s1.fromHash(s.getHash());

      assert(s == s1);
      states.insert(s.getHash());
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
  int numStates = 0;
  for (const auto& d : savedStateMap) {
    if (abs(d.second.bestValue) > 100000) {
      numStates++;
    }
  }
  out << numStates << " 20 1" << endl;
  for (const auto& d : savedStateMap) {
    if (abs(d.second.bestValue) <= 100000) continue;
    int board[4][5] = { 0 };
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
        out << board[i][j] << (i == (height-1) && j == (width-1) ? "" : " ");
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

void trainNeuralNet(const unsigned int width, const unsigned int height, const std::string& fileName) {
  const float learning_rate = 0.01f;
  const float learning_momentum = 0.9f;
  const unsigned int layers[] = { width*height, 1000000, 1 };
  const float desired_error = 0.01f;
  const unsigned int max_neurons = 1000;
  const unsigned int max_iterations = 1;
  const unsigned int iterations_between_reports = 1;

  cout << endl << "Creating network." << endl;

  FANN::neural_net net;
  net.create_standard_array(sizeof(layers)/sizeof(*layers), layers);
  net.set_training_algorithm(FANN::TRAIN_INCREMENTAL);
  net.set_learning_rate(learning_rate);
  net.set_learning_momentum(learning_momentum);
  net.set_activation_function_hidden(FANN::LINEAR);

  net.set_bit_fail_limit(0.0);
  net.randomize_weights(-0.25, 0.25);

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

    net.save(NNET_FILE);
  }
}

void dumpErrors(const int width, const int height, const string& fileName) {
  StateMap_t savedStateMap = loadStateMap(fileName);
  for (const auto& d : savedStateMap) {
    State s(width, height);
    s.fromHash(d.first);
    const int goodness = s.getGoodness(Player::WHITE);
    cout << d.second.bestValue << " " << goodness << endl;
  }
}

void runTests(const int width, const int height, const std::string& fileName) {
  //createTrainData(width, height, fileName);
  trainNeuralNet(width, height, fileName);
  //dumpErrors(width, height, fileName);
}

void playServer(const int width, const int height, const int maxDepth, const bool isWhite, const std::string& gameId, const string& hostName, const int port) {
  static const int max_length = 10;

  char line[256];
  TCPConnector* connector = new TCPConnector();
  TCPStream* stream = connector->connect(hostName.c_str(), port);
  if (!stream) {
    return;
  }
  string message = gameId + (isWhite ? " white\r" : " black\r");
  stream->send(message.c_str(), message.size());
  int len = stream->receive(line, sizeof(line));
  line[len] = 0;

  Game game(width, height, maxDepth);
  const Player player = isWhite ? Player::WHITE : Player::BLACK;

  while (game.getWinner() == Player::NONE) {
    Timer t;
    if (game.getCurrTurn() == player) {
      shared_ptr<Move> bestMove = game.getBestMove();
      string res = "N/A";
      if (bestMove) {
        res = bestMove->toString();
        cout << "Sending: " << res << endl;
        game.move(res, true);
        res += "\r";
        stream->send(res.c_str(), res.size());
      }
    } else {
      len = stream->receive(line, sizeof(line));
      line[len-1] = 0; // remove \r
      message = string(line);
      if (message.find("Timeout") != string::npos) {
        cout << "Timeout" << endl;
        break;
      }
      cout << "Received: " << line << endl;
      game.move(line, false);
    }
    if (game.isDraw()) {
      cout << "Draw by 3-fold repetition" << endl;
      break;
    }
  }

  delete stream;
  delete connector;
}

int main(int argc, char* const argv[]) {
  bool isAuto = false;
  bool isWhite = true;
  bool isSmallBoard = true;
  bool isGenMode = false;
  bool isPopMode = false;
  bool isTestMode = false;
  bool useServer = false;
  int maxDepth = 8;
  string stateMapFileName;
  string gameId;
  string hostName = "tr5130gu-10";
  int hostPort = 12345;
  char c = '\0';
  while ((c = getopt(argc, argv, "abd:glhp:t:s:H:P:")) != -1) {
    switch (c) {
      case 'a':
        isAuto = true;
        break;
      case 'H':
        hostName = optarg;
        break;
      case 'P':
        hostPort = atoi(optarg);
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
        cout << "Usage: " << argv[0] << endl
             << "\t-H <hostname>\tHostname of gameserver. Default is localhost." << endl
             << "\t-P <port>\tPort of gameserver. Default is 12345." << endl
             << "\t-a\t\tAuto-mode. Play against itself." << endl
             << "\t-b\t\tPlay as black. Default is white." << endl
             << "\t-d <depth>\tMax depth. Default is 8." << endl
             << "\t-l\t\tUse large board. Default is small board." << endl
             << "\t-g\t\tGenerate states." << endl
             << "\t-p <statemap>\tPopulate states." << endl
             << "\t-s <gameID>\tUse game server. Default is false." << endl
             << "\t-h\t\tDisplay this help message." << endl;
        return 1;
      case 'p':
        isPopMode = true;
        stateMapFileName = optarg;
        break;
      case 's':
        useServer = true;
        gameId = optarg;
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
  } else if (useServer) {
    playServer(width, height, maxDepth, isWhite, gameId, hostName, hostPort);
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
