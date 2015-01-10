#ifndef INCLUDED_STATE_H
#define INCLUDED_STATE_H

#include <cstddef>
#include <vector>
#include <iostream>
#include <sstream>
using namespace std;

#define NUM_PIECES_PER_SIDE 4
#define WHITE_CHAR '0'
#define BLACK_CHAR '1'

enum Direction {
  N = 0,
  S = 1,
  E = 2,
  W = 3
};

enum Player {
  NONE = 0,
  WHITE = 1,
  BLACK = 2
};

struct Piece {
  Piece() : x(0), y(0) {}
  Piece(int x_, int y_) : x(x_), y(y_) {}
  int x;
  int y;
};

struct Move {
  Piece* piece;
  Direction dir;
  string toString() const {
    static const char dirStr[4] = { 'N', 'S', 'E', 'W' };
    stringstream ss;
    ss << piece->x << piece->y << dirStr[dir];
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

    vector<Move> getMoves(Player player) const {
      vector<Move> v;
      return v;
    }

    bool move(const int x, const int y, const Direction dir) {
      if (x < 1 || x > m_width || y < 1 || y > m_height) return false;
      return movePiece(findPiece(x, y), dir);
    }

    bool movePiece(Piece* const piece, const Direction dir) {
      if (!piece) return false;
      if (Direction::N == dir) {
        if (piece->y == 1 || findPiece(piece->x, piece->y-1)) return false;
        piece->y -= 1;
      } else if (Direction::S == dir) {
        if (piece->y == m_height || findPiece(piece->x, piece->y+1)) return false;
        piece->y += 1;
      } else if (Direction::E == dir) {
        if (piece->x == m_width || findPiece(piece->x+1, piece->y)) return false;
        piece->x += 1;
      } else if (Direction::W == dir) {
        if (piece->x == 0 || findPiece(piece->x-1, piece->y)) return false;
        piece->x -= 1;
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
    }

    int getGoodness(Player player) const {
      return 0;
    }

  private:
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

    Piece* findPiece(const int x, const int y) {
      for (vector<Piece>::iterator it = m_pieces.begin(); it != m_pieces.end(); ++it) {
        Piece& piece = *it;
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
