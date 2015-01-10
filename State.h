#ifndef INCLUDED_STATE_H
#define INCLUDED_STATE_H

#include <cstddef>
#include <vector>
using namespace std;

#define NUM_PIECES 8

enum Direction {
  N = 0,
  S = 1,
  E = 2,
  W = 3
};

enum Status {
  IN_PROGRESS = 0,
  WHITE_WON = 1,
  BLACK_WON = 2
};

struct Piece {
  Piece() : x(0), y(0) {}
  Piece(int x_, int y_) : x(x_), y(y_) {}
  int x;
  int y;
};

class State {
  public:
    State(int width, int height) : m_width(width), m_height(height) {
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

    bool move(const int x, const int y, const Direction dir) {
      if (x < 1 || x > m_width || y < 1 || y > m_height) return false;
      return movePiece(findPiece(x, y), dir);
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

    bool movePiece(Piece* piece, const Direction dir) {
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

    Status getStatus() const {
      if (hasWhiteWon()) return Status::WHITE_WON;
      else if (hasBlackWon()) return Status::BLACK_WON;
      else return Status::IN_PROGRESS;
    }

    bool hasWhiteWon() const {
      vector<Piece> pieces(m_pieces.begin(), m_pieces.begin()+4);
      for (int i = 0; i < 4; ++i) {
        vector<Piece> tmp = pieces; // copy
        tmp.erase(tmp.begin() + i);
        if (isConnected(tmp)) {
          return true;
        }
      }
      return false;
    }

    bool hasBlackWon() const {
      vector<Piece> pieces(m_pieces.begin()+4, m_pieces.end());
      for (int i = 0; i < 4; ++i) {
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

  private:
    vector<Piece> m_pieces; // 0-3 are white, 4-7 are black
    int m_width;
    int m_height;
};

#endif
