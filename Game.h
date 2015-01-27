#ifndef INCLUDED_GAME_H
#define INCLUDED_GAME_H

#include "State.h"
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
      int numExpanded = 0;
      for (auto& move : moves) {
        pushState(currState);

        currState.move(move.x, move.y, move.dir, true);
        if (currState.hasPlayerWon(currTurn)) {
          bestMove = make_shared<Move>(move);
          bestWorst = numeric_limits<int>::max();
          currState = popState();
          break;
        } else {
          goodness = evaluate(currState, currTurn, maxDepth, -numeric_limits<int>::max(), -bestWorst, numExpanded);
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
      cout << "bestWorst: " << bestWorst << ", numExpanded: " << numExpanded << endl;

      return bestMove;
    }

    int evaluate(State& s, const Player player, const int currDepth, int alpha, int beta, int& numExpanded) {
      const int alphaOrig = alpha;
      const Hash_t hash = s.getHash();
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
        vector<Move> moves = s.getMoves(OTHER(player));
        for (auto& move : moves) {
          pushState(s);

          s.move(move.x, move.y, move.dir, true);
          int goodness = evaluate(s, OTHER(player), currDepth-1, -beta, -alpha, ++numExpanded);
          if (goodness > bestVal) {
            bestVal = goodness;
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


#endif
