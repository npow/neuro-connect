#ifndef INCLUDED_GAME_H
#define INCLUDED_GAME_H

#include <map>
#include <random>
#include "State.h"
using namespace std;

#define USE_AB_PRUNING 1
static mt19937 rng(42);

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

    int playToEnd(State& s, const Player player, const int currDepth, int& leavesReached) {
      const Player winner = s.getWinner();
      if (winner != Player::NONE) {
        leavesReached++;
        return (player == winner ? 1 : -1) * numeric_limits<int>::max();
      } else if (checkIsGameDrawn(s)) {
        leavesReached++;
        return 0;
      }

      vector<Move> moves = s.getMoves(s.getCurrTurn());
      uniform_int_distribution<int> uni(0, (int)moves.size());
      const int index = uni(rng);
      pushState(s);

      Move move = moves[index];
      s.move(move.x, move.y, move.dir, false);
      int goodness = playToEnd(s, player, currDepth, leavesReached);
      s = popState();
      return goodness;
    }

    shared_ptr<Move> getBestMoveMonteCarlo() {
      int leavesReached = 0;
      clock_t startTime = clock();
      const vector<Move> moves = currState.getMoves(currTurn);
      map<Move, int> goodnessMap;
      uniform_int_distribution<int> uni(0, (int)moves.size()-1);
      while (true) {
        const double elapsedTime = ((clock() - startTime)/(double)CLOCKS_PER_SEC);
        if (elapsedTime > 9.0) break;

        pushState(currState);

        const int index = uni(rng);
        const Move move = moves[index];

        currState.move(move.x, move.y, move.dir, true);

        const int goodness = playToEnd(currState, currTurn, 10, leavesReached);
        if (goodnessMap.find(move) == goodnessMap.end()) {
          goodnessMap[move] = 0;
        }
        goodnessMap[move] += goodness;

        currState = popState();
      }
      cout << "leaves reached: " << leavesReached << endl;

      shared_ptr<Move> bestMove;
      int bestValue = -numeric_limits<int>::max();
      for (const auto& p : goodnessMap) {
        cout << "goodness: " << p.second << endl;
        if (p.second > bestValue) {
          bestValue = p.second;
          bestMove = make_shared<Move>(p.first);
        }
      }
      return bestMove;
    }

    shared_ptr<Move> getBestMove() {
#ifdef USE_MONTECARLO
      return getBestMoveMC();
#endif
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
#ifdef USE_MINIMAX
          goodness = (currTurn == Player::BLACK ? -1 : 1) * minimax(currState, maxDepth, -numeric_limits<int>::max(), numeric_limits<int>::max(), numExpanded);
#else
          goodness = negamax(currState, currTurn, maxDepth, -numeric_limits<int>::max(), -bestWorst, numExpanded);
#endif

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

    int minimax(State& s, const int maxDepth, int alpha, int beta, int& numExpanded) {
      return maxValue(s, maxDepth, alpha, beta, numExpanded);
    }

    int maxValue(State& s, const int currDepth, int alpha, int beta, int& numExpanded) {
      ++numExpanded;
      const Player winner = s.getWinner();
      if (winner != Player::NONE) {
        return (winner == Player::WHITE ? 1 : -1) * numeric_limits<int>::max();
      } else if (checkIsGameDrawn(s)) {
        return 0;
      } else if (currDepth == 0) {
        return s.getGoodness(Player::WHITE);
      }
      int v = -numeric_limits<int>::max();
      vector<Move> moves = s.getMoves(s.getCurrTurn());
      for (const auto& move : moves) {
        pushState(s);
        s.move(move.x, move.y, move.dir, true);

        v = max(v, minValue(s, currDepth-1, alpha, beta, numExpanded));
#if USE_AB_PRUNING
        if (v >= beta) {
          s = popState();
          return v;
        }
#endif
        alpha = max(alpha, v);
        s = popState();
      }
      return v;
    }

    int minValue(State& s, const int currDepth, int alpha, int beta, int& numExpanded) {
      ++numExpanded;
      const Player winner = s.getWinner();
      if (winner != Player::NONE) {
        return (winner == Player::WHITE ? 1 : -1) * numeric_limits<int>::max();
      } else if (checkIsGameDrawn(s)) {
        return 0;
      } else if (currDepth == 0) {
        return s.getGoodness(Player::WHITE);
      }
      int v = numeric_limits<int>::max();
      vector<Move> moves = s.getMoves(s.getCurrTurn());
      for (const auto& move : moves) {
        pushState(s);
        s.move(move.x, move.y, move.dir, true);

        v = min(v, maxValue(s, currDepth-1, alpha, beta, numExpanded));
#if USE_AB_PRUNING
        if (v <= alpha) {
          s = popState();
          return v;
        }
#endif
        beta = min(beta, v);
        s = popState();
      }
      return v;
    }

    int negamax(State& s, const Player player, const int currDepth, int alpha, int beta, int& numExpanded) {
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
          int goodness = negamax(s, OTHER(player), currDepth-1, -beta, -alpha, ++numExpanded);
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
