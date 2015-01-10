#ifndef INCLUDED_STATE_H
#define INCLUDED_STATE_H

#include <algorithm>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <sstream>
#include <vector>
using namespace std;

#define NUM_PIECES_PER_SIDE 4
#define WHITE_CHAR '0'
#define BLACK_CHAR '1'
#define OTHER(player) ((player) == Player::WHITE ? Player::BLACK : Player::WHITE)

enum Direction {
  N = 0,
  S = 1,
  E = 2,
  W = 3,
  END = 4
};

enum Player {
  NONE = 0,
  WHITE = 1,
  BLACK = 2
};

struct Piece {
  Piece() : x(0), y(0) {}
  Piece(int x, int y) : x(x), y(y) {}
  int x;
  int y;
};

struct Move {
  Move() : piece(NULL), dir(Direction::END) {}
  Move(Piece* const piece, Direction dir) : piece(piece), dir(dir) {}

  Piece* piece;
  Direction dir;
  string toString() const {
    static const char dirStr[4] = { 'N', 'S', 'E', 'W' };
    stringstream ss;
    if (piece) {
      ss << piece->x << piece->y << dirStr[dir];
    }
    return ss.str();
  }
};

class State {
  public:
    State(const int width, const int height) : m_width(width), m_height(height) {
      const int offset = (7 == width && 6 == height ? 1 : 0);

      m_pieces.push_back(Piece(1+offset, 1+offset));
      m_pieces.push_back(Piece(5+offset, 2+offset));
      m_pieces.push_back(Piece(1+offset, 3+offset));
      m_pieces.push_back(Piece(5+offset, 4+offset));

      m_pieces.push_back(Piece(5+offset, 1+offset));
      m_pieces.push_back(Piece(1+offset, 2+offset));
      m_pieces.push_back(Piece(5+offset, 3+offset));
      m_pieces.push_back(Piece(1+offset, 4+offset));
    }

    ~State() {
    }

    vector<Move> getMoves(Player player) {
      const int offset = player == Player::WHITE ? 0 : NUM_PIECES_PER_SIDE;
      vector<Move> v;
      for (int i = 0; i < NUM_PIECES_PER_SIDE; ++i) {
        Piece& piece = m_pieces[offset + i];
        for (int dir = Direction::N; dir != Direction::END; ++dir) {
          if (isValidMove(&piece, static_cast<Direction>(dir))) {
            v.push_back(Move(&piece, static_cast<Direction>(dir)));
          }
        }
      }
      return v;
    }

    bool move(const int x, const int y, const Direction dir) {
      if (x < 1 || x > m_width || y < 1 || y > m_height) return false;
      return movePiece(findPiece(x, y), dir);
    }

    bool movePiece(Piece* const piece, const Direction dir) {
      if (!piece || !isValidMove(piece, dir)) return false;
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
      if (hasWhiteWon()) return Player::WHITE;
      else if (hasBlackWon()) return Player::BLACK;
      else return Player::NONE;
    }

    bool hasWhiteWon() const {
      vector<Piece> pieces(m_pieces.begin(), m_pieces.begin()+NUM_PIECES_PER_SIDE);
      return hasPlayerWon(pieces);
    }

    bool hasBlackWon() const {
      vector<Piece> pieces(m_pieces.begin()+NUM_PIECES_PER_SIDE, m_pieces.end());
      return hasPlayerWon(pieces);
    }

    void print() const {
      cout << "==========" << endl;
      char grid[m_height][m_width];
      for (int i = 0; i < m_height; ++i) {
        for (int j = 0; j < m_width; ++j) {
          grid[i][j] = '_';
        }
      }
      for (int i = 0; i < m_pieces.size(); ++i) {
        const bool isWhite = (i < NUM_PIECES_PER_SIDE);
        const Piece& piece = m_pieces[i];
        grid[piece.y-1][piece.x-1] = (isWhite ? WHITE_CHAR : BLACK_CHAR);
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
      return getNumRuns(player) - getNumRuns(OTHER(player));
    }

  private:
    int getNumRuns(const Player player) const {
      const auto ps = getCombinations(NUM_PIECES_PER_SIDE, 2);
      const int offset = player == Player::WHITE ? 0 : NUM_PIECES_PER_SIDE;
      int numRuns = 0;
      for (const auto& p : ps) {
        const auto& A = m_pieces[offset + p.first];
        const auto& B = m_pieces[offset + p.second];
        if (isAdjacent(A.x, A.y, B.x, B.y)) {
          numRuns++;
        }
      }
      return numRuns;
    }

    bool hasPlayerWon(const vector<Piece>& pieces) const {
      for (int i = 0; i < NUM_PIECES_PER_SIDE; ++i) {
        vector<Piece> tmp = pieces; // copy
        tmp.erase(tmp.begin() + i);
        if (isConnected(tmp)) {
          return true;
        }
      }
      return false;
    }

    bool isConnected(const vector<Piece>& pieces) const {
      const Piece& A = pieces[0];
      const Piece& B = pieces[1];
      const Piece& C = pieces[2];
      return isCollinear(A.x, A.y, B.x, B.y, C.x, C.y);
    }

    bool isCollinear(int x1, int y1, int x2, int y2, int x3, int y3) const {
      return (y1 - y2) * (x1 - x3) == (y1 - y3) * (x1 - x2);
    }

    bool isAdjacent(const int x1, const int y1, const int x2, const int y2) const {
      const int dx = abs(x1 - x2);
      const int dy = abs(y1 - y2);
      const int dist = dx + dy;
      return dist == 1 || dist == 2;
    }

    vector< pair<int, int> > getCombinations(const int n, const int r) const {
      vector< pair<int, int> > res;

      vector<bool> v(n);
      fill(v.begin() + n - r, v.end(), true);

      do {
        pair<int, int> p(-1, -1);
        for (int i = 0; i < n; ++i) {
          if (v[i]) {
            (p.first == -1 ? p.first : p.second) = i;
          }
        }
        res.push_back(p);
      } while (next_permutation(v.begin(), v.end()));

      return res;
    }

    Piece* findPiece(const int x, const int y) {
      return const_cast<Piece*>(static_cast<const State*>(this)->findPiece(x, y));
    }

    const Piece* findPiece(const int x, const int y) const {
      for (const auto& piece : m_pieces) {
        if (piece.x == x && piece.y == y) {
          return &piece;
        }
      }
      return NULL;
    }

  private:
    vector<Piece> m_pieces; // 0-3 are white, 4-7 are black
    int m_width;
    int m_height;
};

#endif
