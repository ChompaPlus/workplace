#pragma once

#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <mutex>
#include <tuple>
#include <iostream>


const int BOARD_SIZE = 15; // 棋盘大小

const std::string EMPTY = " "; // 空单元格
const std::string AI_PLAYER = "white"; // AI白
const std::string HUMAN_PLAYER = "black"; // 用户黑

class AiGame
{
private:
    std::vector<std::vector<std::string>> board_; // 棋盘
    mutable std::mutex mutex_; // 互斥锁 mutable关键字可以让const成员函数修改成员变量
    bool gameOver_; // 游戏是否结束
    int userId_; // 用户ID
    int moveCount_; // 移动次数
    std::string winner_{"none"};
    std::pair<int, int> lastMove_{-1, -1}; // 上一步的位置
public:
    AiGame(int userId);

    // 判断是否平局
    bool isDraw() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return moveCount_ >= BOARD_SIZE * BOARD_SIZE;
    }
    
    bool humanMove(int x, int y); // 用户落子

    bool checkwin(int x, int y, const std::string& player); // 判断是否胜利

    void aiMove(); // AI落子

    std::pair<int, int> getLastMove() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return lastMove_;
    }
    // 获取棋盘状态
    std::vector<std::vector<std::string>> getBoard() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return board_;
    }

    // 获取游戏是否结束
    bool isGameOver() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return gameOver_;
    }

    // 获胜者
    std::string getWinner() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return winner_;
    }
private:
    // 检查移动是否有效
    bool isValidMove(int x, int y) const
    {
        if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) return false;
        if (board_[x][y] != EMPTY) return false;
        if (gameOver_ || isDraw()) return false;
        return true;
    }
    // 检查棋子是否在棋盘上
    bool isInBoard(int x, int y) const
    {
        return x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE;
    }
    // 评估威胁
    int evaluateThreat(int r, int c);
    // 获取人机的最佳移动位置
    std::pair<int, int> getBestMove();
    // 检查位置是否靠近已有棋子
    bool isNearOccupied(int r, int c);
    //
    int minimax(std::vector<std::vector<std::string>>& board, int depth, bool maximizing, int alpha, int beta);
    // 评估棋盘
    int evaluateBoard(const std::vector<std::vector<std::string>>& board, const std::string& player);

};