#include "../include/AiGame.h"
#include <muduo/base/Logging.h>
#include <chrono>
#include <thread>


constexpr int MAX_DEPTH = 3;

AiGame::AiGame(int userId) : gameOver_(false), 
                            userId_(userId), 
                            moveCount_(0), 
                            lastMove_(-1, -1), 
                            board_(BOARD_SIZE, std::vector<std::string>(BOARD_SIZE, EMPTY))
{
    srand(time(0)); // 初始化随机数种子, 用于AI的随机落子
}

bool AiGame::humanMove(int x, int y)
{
    if (!isValidMove(x, y)) {return false; LOG_ERROR << "无效的移动";}
    board_[x][y] = HUMAN_PLAYER;
    lastMove_ = {x, y};
    moveCount_++;

    if (checkwin(x, y, HUMAN_PLAYER))
    {
        gameOver_ = true;
        winner_ = "human";
    }
    return true;

}

bool AiGame::checkwin(int x, int y, const std::string& player)
{
    // 检查行、列、对角线、反对角线
    const int dx[] = {1, 0, 1, 1};
    const int dy[] = {0, 1, 1, -1};

    for (int dir = 0; dir < 4; ++dir)
    {
        int count = 1;
        
        // 正向
        for (int i = 1; i < 5; ++i)
        {
            int nx = x + i * dx[dir];
            int ny = y + i * dy[dir];
            if (!isInBoard(nx, ny) || board_[nx][ny] != player) break;
            count++;
        }

        // 反向
        for (int i = 1; i < 5; ++i)
        {
            int nx = x - i * dx[dir];
            int ny = y - i * dy[dir];
            if (!isInBoard(nx, ny) || board_[nx][ny]!= player) break;
            count++;
        }
        if (count >= 5) return true;
    }
    return false;
}

void AiGame::aiMove()
{
    if (gameOver_ || isDraw()) return;
    // 简单的随机落子
    std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟AI思考时间
    int x, y;
    // 选择最佳位置进行移动
    std::tie(x, y) = getBestMove();
    board_[x][y] = AI_PLAYER;
    moveCount_++;
    lastMove_ = {x, y};

    // check win
    if (checkwin(x, y, AI_PLAYER))
    {
        gameOver_ = true;
        winner_ = "ai";
    }
}

// 评估威胁
int AiGame::evaluateThreat(int r, int c)
{
    int threat = 0;
    const int dir[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
    for (int d = 0; d < 4; ++d)
    {
        int count = 1;
        for (int i = 1; i <= 2; i++)
        {
            int x = r + i * dir[d][0];
            int y = c + i * dir[d][1];
            if (x > 0 && x < BOARD_SIZE && y > 0 && y < BOARD_SIZE && board_[x][y] == HUMAN_PLAYER)
            {
                count++;
            }
        }
        threat += count;
    }
    return threat;
}

// 评估位置
std::pair<int, int> AiGame::getBestMove()
{
    // std::pair<int, int> bestMove = {-1, -1};
    // int maxThreat = 1;
    // // 遍历了正张图像的所有位置
    // for (int i = 0; i < BOARD_SIZE; ++i)
    // {
    //     for (int j = 0; j < BOARD_SIZE; ++j)
    //     {
    //         if (board_[i][j] == EMPTY)
    //         {
    //             board_[i][j] = AI_PLAYER;
    //             if (checkwin(i, j, AI_PLAYER))
    //             {
    //                 return {i, j};
    //             }
    //             board_[i][j] = EMPTY;

    //             // 模拟玩家落子，判断是否需要防守
    //             board_[i][j] = HUMAN_PLAYER;
    //             if (checkwin(i, j, HUMAN_PLAYER))
    //             {
    //                 board_[i][j] = AI_PLAYER;
    //                 return {i, j};// 立刻防守
    //             }
    //             board_[i][j] = EMPTY;
    //         }
    //     }
    // }
    // // 评估每个空位的威胁程度，选择最佳防守的位置
    // for (int i = 0; i < BOARD_SIZE; ++i)
    // {
    //     for (int j = 0; j < BOARD_SIZE; ++j)
    //     {
    //         if (board_[i][j] == EMPTY)
    //         {
    //             int threat = evaluateThreat(i, j);
    //             if (threat > maxThreat)
    //             {
    //                 maxThreat = threat;
    //                 bestMove = {i, j};
    //             }
    //         }
    //     }
    // }

    // // 如果没有合适的位置，选择一个靠近玩家或者已有棋子的空位
    // if (bestMove.first == -1 && bestMove.second == -1)
    // {
    //     std::vector<std::pair<int, int>> candidates;
    //     for (int i = 0; i < BOARD_SIZE; ++i)
    //     {
    //         for (int j = 0; j < BOARD_SIZE; ++j)
    //         {
    //             if (board_[i][j] == EMPTY)
    //             {
    //                 if (isNearOccupied(i, j))
    //                 {
    //                     candidates.push_back({i, j});
    //                 }
    //             }
    //         }
    //     }
        
    //     if (!candidates.empty())
    //     {
    //         board_[candidates[rand() % candidates.size()].first][candidates[rand() % candidates.size()].second] = AI_PLAYER;
    //         return candidates[rand() % candidates.size()];
    //     }
    // }

    // // 如果所有策略都无效，随机选择一个空位
    // if (bestMove.first == -1 && bestMove.second == -1)
    // {
    //     std::vector<std::pair<int, int>> emptyCells;
    //     for (int i = 0; i < BOARD_SIZE; ++i)
    //     {
    //         for (int j = 0; j < BOARD_SIZE; ++j)
    //         {
    //             if (board_[i][j] == EMPTY)
    //             {
    //                 emptyCells.push_back({i, j});
    //             }
    //         }
    //     }
    //     if (!emptyCells.empty())
    //     {
    //         board_[emptyCells[rand() % emptyCells.size()].first][emptyCells[rand() % emptyCells.size()].second] = AI_PLAYER;
    //         return emptyCells[rand() % emptyCells.size()];
    //     }
    // }
    int bestScore = std::numeric_limits<int>::min();
    std::pair<int, int> bestMove = {-1, -1};

    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board_[i][j] == EMPTY && isNearOccupied(i, j)) {
                board_[i][j] = AI_PLAYER;
                int score = minimax(board_, MAX_DEPTH - 1, false, std::numeric_limits<int>::min(), std::numeric_limits<int>::max());
                board_[i][j] = EMPTY;

                if (score > bestScore) {
                    bestScore = score;
                    bestMove = {i, j};
                }
            }
        }
    }
    return bestMove;
}

bool AiGame::isNearOccupied(int r, int c)
{
    const int dir[8][2] = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};
    for (int i = 0; i < 8; ++i)
    {
        int x = r + dir[i][0];
        int y = c + dir[i][1];
        if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE && board_[x][y] != EMPTY)
        {
            return true;
        }

    }
    return false;
}
int AiGame::evaluateBoard(const std::vector<std::vector<std::string>> &board, const std::string &player)
{
    int score = 0;
    const int dx[] = {1, 0, 1, 1};
    const int dy[] = {0, 1, 1, -1};

    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (board[x][y] != player) continue;
            for (int d = 0; d < 4; ++d) {
                int count = 1;
                for (int i = 1; i < 5; ++i) {
                    int nx = x + i * dx[d];
                    int ny = y + i * dy[d];
                    if (!AiGame::isInBoard(nx, ny) || board[nx][ny] != player) break;
                    count++;
                }
                if (count == 2) score += 10;
                else if (count == 3) score += 100;
                else if (count == 4) score += 1000;
                else if (count >= 5) score += 100000;
            }
        }
    }
    return score;
}

int AiGame::minimax(std::vector<std::vector<std::string>>& board, int depth, bool maximizing, int alpha, int beta) {
    if (depth == 0) {
        return evaluateBoard(board, AI_PLAYER) - evaluateBoard(board, HUMAN_PLAYER);
    }

    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    std::string currentPlayer = maximizing ? AI_PLAYER : HUMAN_PLAYER;

    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board[i][j] == EMPTY) {
                board[i][j] = currentPlayer;

                int score = minimax(board, depth - 1, !maximizing, alpha, beta);
                board[i][j] = EMPTY;

                if (maximizing) {
                    best = std::max(best, score);
                    alpha = std::max(alpha, best);
                } else {
                    best = std::min(best, score);
                    beta = std::min(beta, best);
                }
                if (beta <= alpha) return best;
            }
        }
    }

    return best;
}