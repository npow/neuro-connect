#ifndef INCLUDED_STATE_H
#define INCLUDED_STATE_H

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <sstream>
#include <vector>
#include <redisclient.h>
using namespace std;

#define NUM_PIECES_PER_SIDE 4
#define WHITE_CHAR '0'
#define BLACK_CHAR '1'
#define OTHER(player) ((player) == Player::WHITE ? Player::BLACK : Player::WHITE)
#define toInt(c) (c - '0')

static boost::shared_ptr<redis::client> init_non_cluster_client() {
  const char* c_host = getenv("REDIS_HOST");
  string host = "localhost";
  if (c_host) {
    host = c_host;
  }
  return boost::shared_ptr<redis::client>(new redis::client(host));
}

static redis::client& getRedisClient() {
  static boost::shared_ptr<redis::client> shared_c;
  if (!shared_c) {
    shared_c = init_non_cluster_client();
  }
  redis::client& c = *shared_c;
  return c;
}

class Timer {
  public:
    Timer() {
      m_start = clock();
    }
    ~Timer() {
      double duration = (clock() - m_start) / (double)CLOCKS_PER_SEC;
      cout << "Took: " << duration << "s" << endl;
    }

  private:
    clock_t m_start;
};

static vector< vector<int> > getCombinations(const int n, const int r) {
  vector< vector<int> > res;

  vector<bool> v(n);
  fill(v.begin() + n - r, v.end(), true);

  do {
    vector<int> p;
    p.reserve(r);
    for (int i = 0; i < n; ++i) {
      if (v[i]) {
        p.push_back(i);
      }
    }
    res.push_back(p);
  } while (next_permutation(v.begin(), v.end()));

  return res;
}

static const vector< vector<int> >& getCombinations_8_4() {
  static vector< vector<int> > v;
  if (v.empty()) {
    v = getCombinations(8, 4);
  }
  return v;
}

static const vector< vector<int> >& getCombinations_4_3() {
  static vector< vector<int> > v;
  if (v.empty()) {
    v = getCombinations(NUM_PIECES_PER_SIDE, 3);
  }
  return v;
}

static const vector< vector<int> >& getCombinations_4_2() {
  static vector< vector<int> > v;
  if (v.empty()) {
    v = getCombinations(NUM_PIECES_PER_SIDE, 2);
  }
  return v;
}

enum Direction {
  N = 0,
  S = 1,
  E = 2,
  W = 3,
  END = 4
};

enum Player {
  WHITE = 0,
  BLACK = 1,
  NONE = 2
};

struct Piece {
  Piece() : x(0), y(0) {}

  Piece(int x, int y) : x(x), y(y) {}

  bool operator==(const Piece& rhs) const {
    return x == rhs.x && y == rhs.y;
  }

  bool operator!=(const Piece& rhs) const {
    return !(this->operator==(rhs));
  }

  bool operator<(const Piece& rhs) const {
    if (x < rhs.x) {
      return true;
    } else if (x == rhs.x) {
      if (y < rhs.y) {
        return true;
      }
    } else {
      return y < rhs.y;
    }
    return false;
  }

  int x;
  int y;
};

struct Move {
  Move() : x(0), y(0), dir(Direction::END) {}
  Move(const int x, const int y, Direction dir) : x(x), y(y), dir(dir) {}

  int x;
  int y;
  Direction dir;
  string toString() const {
    static const char dirStr[4] = { 'N', 'S', 'E', 'W' };
    stringstream ss;
    if (x != 0 && y != 0 && dir != Direction::END) {
      ss << x << y << dirStr[dir];
    }
    return ss.str();
  }
};

class State {
  public:
    State(const int width, const int height) : m_width(width), m_height(height) {
      const int offset = (7 == width && 6 == height ? 1 : 0);

      getPieces(Player::WHITE).push_back(Piece(1+offset, 1+offset));
      getPieces(Player::WHITE).push_back(Piece(5+offset, 2+offset));
      getPieces(Player::WHITE).push_back(Piece(1+offset, 3+offset));
      getPieces(Player::WHITE).push_back(Piece(5+offset, 4+offset));

      getPieces(Player::BLACK).push_back(Piece(5+offset, 1+offset));
      getPieces(Player::BLACK).push_back(Piece(1+offset, 2+offset));
      getPieces(Player::BLACK).push_back(Piece(5+offset, 3+offset));
      getPieces(Player::BLACK).push_back(Piece(1+offset, 4+offset));
    }

    ~State() {
    }

    void setPieces(const vector<Piece>& whitePieces, const vector<Piece>& blackPieces) {
      getPieces(Player::WHITE) = whitePieces;
      getPieces(Player::BLACK) = blackPieces;
    }

    bool operator==(const State& rhs) const {
      return isEqual(getPieces(Player::WHITE), rhs.getPieces(Player::WHITE)) &&
             isEqual(getPieces(Player::BLACK), rhs.getPieces(Player::BLACK));
    }

    bool operator!=(const State& rhs) const {
      return !(this->operator==(rhs));
    }

    vector<Piece>& getPieces(const Player player) {
      return m_pieces[static_cast<int>(player)];
    }

    const vector<Piece>& getPieces(const Player player) const {
      return m_pieces[static_cast<int>(player)];
    }

    vector<Move> getMoves(const Player player) const {
      vector<Move> v;
      v.reserve(8);
      for (auto& piece : getPieces(player)) {
        for (int dir = Direction::N; dir != Direction::END; ++dir) {
          if (isValidMove(&piece, static_cast<Direction>(dir))) {
            v.push_back(Move(piece.x, piece.y, static_cast<Direction>(dir)));
          }
        }
      }
      return v;
    }

    bool move(const int x, const int y, const Direction dir, bool skipVerification = false) {
      if (x < 1 || x > m_width || y < 1 || y > m_height) return false;
      return movePiece(findPiece(x, y), dir, skipVerification);
    }

    bool movePiece(Piece* const piece, const Direction dir, bool skipVerification) {
      if (!piece || (!skipVerification && !isValidMove(piece, dir))) return false;
      if (Direction::N == dir) {
        piece->y -= 1;
      } else if (Direction::S == dir) {
        piece->y += 1;
      } else if (Direction::E == dir) {
        piece->x += 1;
      } else if (Direction::W == dir) {
        piece->x -= 1;
      }
      return true;
    }

    bool isValidMove(const Piece* const piece, const Direction dir) const {
      if (!piece) return false;
      if (Direction::N == dir) {
        if (piece->y == 1 || findPiece(piece->x, piece->y-1)) return false;
      } else if (Direction::S == dir) {
        if (piece->y == m_height || findPiece(piece->x, piece->y+1)) return false;
      } else if (Direction::E == dir) {
        if (piece->x == m_width || findPiece(piece->x+1, piece->y)) return false;
      } else if (Direction::W == dir) {
        if (piece->x == 1 || findPiece(piece->x-1, piece->y)) return false;
      }
      return true;
    }

    Player getWinner() const {
      if (hasPlayerWon(getPieces(Player::WHITE))) return Player::WHITE;
      else if (hasPlayerWon(getPieces(Player::BLACK))) return Player::BLACK;
      else return Player::NONE;
    }

    inline bool hasPlayerWon(const Player player) const {
      return hasPlayerWon(getPieces(player));
    }

    void print() const {
      cout << "==========" << endl;
      char grid[m_height][m_width];
      for (int i = 0; i < m_height; ++i) {
        for (int j = 0; j < m_width; ++j) {
          grid[i][j] = '_';
        }
      }
      for (const auto& piece : getPieces(Player::WHITE)) {
        grid[piece.y-1][piece.x-1] = WHITE_CHAR;
      }
      for (const auto& piece : getPieces(Player::BLACK)) {
        grid[piece.y-1][piece.x-1] = BLACK_CHAR;
      }
      for (int i = 0; i < m_height; ++i) {
        for (int j = 0; j < m_width; ++j) {
          cout << (j == 0 ? "" : ",") << grid[i][j];
        }
        cout << endl;
      }
      cout << "==========" << endl;
    }

    int getGoodness(const Player player) const {
      redis::client& client = getRedisClient();
      const string hash = toString(Player::WHITE);
      redis::distributed_int goodness(hash, 0, client);
      return player == Player::WHITE ? goodness : -goodness;
    }

    string toString(const Player player) const {
      stringstream ss;
      auto p1 = getPieces(player); // copy
      sort(p1.begin(), p1.end());
      for (const auto& p : p1) {
        ss << p.x << p.y;
      }
      auto p2 = getPieces(OTHER(player)); // copy
      sort(p2.begin(), p2.end());
      for (const auto& p : p2) {
        ss << p.x << p.y;
      }
      return ss.str();
    }

    void fromString(const string& s) {
      assert(s.length() == 16);
      vector<Piece>& whitePieces = getPieces(Player::WHITE);
      vector<Piece>& blackPieces = getPieces(Player::BLACK);
      int i = 0;
      for (int j = 0; j < whitePieces.size(); ++j) {
        whitePieces[j].x = toInt(s[i++]);
        whitePieces[j].y = toInt(s[i++]);
      }
      for (int j = 0; j < blackPieces.size(); ++j) {
        blackPieces[j].x = toInt(s[i++]);
        blackPieces[j].y = toInt(s[i++]);
      }
    }

  private:
    int getNumRuns(const Player player) const {
      const auto& ps = getCombinations_4_2();
      const vector<Piece>& pieces = getPieces(player);
      int numRuns = 0;
      for (const auto& p : ps) {
        const auto& A = pieces[p[0]];
        const auto& B = pieces[p[1]];
        if (isAdjacent(A.x, A.y, B.x, B.y)) {
          numRuns++;
        }
      }
      return numRuns;
    }

    bool hasPlayerWon(const vector<Piece>& pieces) const {
      const auto& ps = getCombinations_4_3();
      for (const auto& p : ps) {
        if (isConnected(pieces[p[0]], pieces[p[1]], pieces[p[2]])) {
          return true;
        }
      }
      return false;
    }

    bool isConnected(const Piece& A, const Piece& B, const Piece& C) const {
      if (!isCollinear(A.x, A.y, B.x, B.y, C.x, C.y)) {
        return false;
      }
      return (isAdjacent(A.x, A.y, B.x, B.y) && isAdjacent(B.x, B.y, C.x, C.y)) ||
             (isAdjacent(A.x, A.y, C.x, C.y) && isAdjacent(C.x, C.y, B.x, B.y)) ||
             (isAdjacent(C.x, C.y, A.x, A.y) && isAdjacent(A.x, A.y, B.x, B.y)) ||
             (isDiagonallyAdjacent(A.x, A.y, B.x, B.y) && isDiagonallyAdjacent(B.x, B.y, C.x, C.y)) ||
             (isDiagonallyAdjacent(A.x, A.y, C.x, C.y) && isDiagonallyAdjacent(C.x, C.y, B.x, B.y)) ||
             (isDiagonallyAdjacent(C.x, C.y, A.x, A.y) && isDiagonallyAdjacent(A.x, A.y, B.x, B.y));
    }

    bool isCollinear(int x1, int y1, int x2, int y2, int x3, int y3) const {
      return (y1 - y2) * (x1 - x3) == (y1 - y3) * (x1 - x2);
    }

    bool isAdjacent(const int x1, const int y1, const int x2, const int y2) const {
      const int dx = abs(x1 - x2);
      const int dy = abs(y1 - y2);
      return dx + dy == 1;
    }

    bool isDiagonallyAdjacent(const int x1, const int y1, const int x2, const int y2) const {
      const int dx = abs(x1 - x2);
      const int dy = abs(y1 - y2);
      return dx == 1 && dy == 1;
    }

    Piece* findPiece(const int x, const int y) {
      return const_cast<Piece*>(static_cast<const State*>(this)->findPiece(x, y));
    }

    const Piece* findPiece(const int x, const int y) const {
      for (const auto& piece : getPieces(Player::WHITE)) {
        if (piece.x == x && piece.y == y) {
          return &piece;
        }
      }
      for (const auto& piece : getPieces(Player::BLACK)) {
        if (piece.x == x && piece.y == y) {
          return &piece;
        }
      }
      return nullptr;
    }

    inline bool isEqual(const vector<Piece>& v1, const vector<Piece>& v2) const {
      if (v1.size() != v2.size()) return false;
      for (const auto& piece : v1) {
        if (find(v2.begin(), v2.end(), piece) == v2.end()) {
          return false;
        }
      }
      return true;
    }

  private:
    vector<Piece> m_pieces[2];
    int m_width;
    int m_height;
};

#endif
