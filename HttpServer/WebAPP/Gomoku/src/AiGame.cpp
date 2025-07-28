#include "../include/AiGame.h"
#include <muduo/base/Logging.h>
#include <chrono>
#include <thread>
#include <unordered_map>
#include <algorithm>
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
    // std::this_thread::sleep_for(std::chrono::seconds(1)); // 模拟AI思考时间
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
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board_[i][j] == EMPTY) {
                board_[i][j] = AI_PLAYER;
                if (checkwin(i, j, AI_PLAYER)) {
                    board_[i][j] = EMPTY; // 恢复棋盘
                    return {i, j}; // 直接返回胜利点
                }
                board_[i][j] = EMPTY;
            }
        }
    }

    // 2. 检查对手是否有直接胜利点（必须防守）
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board_[i][j] == EMPTY) {
                board_[i][j] = HUMAN_PLAYER;
                if (checkwin(i, j, HUMAN_PLAYER)) {
                    board_[i][j] = EMPTY; // 恢复棋盘
                    return {i, j}; // 防守对手胜利点
                }
                board_[i][j] = EMPTY;
            }
        }
    }

    // 3. 使用启发式排序筛选候选位置（仅保留靠近已有棋子的空位）
    std::vector<std::pair<int, int>> candidates;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board_[i][j] == EMPTY && isNearOccupied(i, j)) {
                candidates.emplace_back(i, j);
            }
        }
    }

    // 4. 对候选位置按评估分排序（优先评估分高的位置，提升剪枝效率）
    std::sort(candidates.begin(), candidates.end(), [this](const auto& a, const auto& b) {
        // 临时模拟落子，计算评估分
        board_[a.first][a.second] = AI_PLAYER;
        int scoreA = evaluateBoard(board_, AI_PLAYER);
        board_[a.first][a.second] = EMPTY;

        board_[b.first][b.second] = AI_PLAYER;
        int scoreB = evaluateBoard(board_, AI_PLAYER);
        board_[b.first][b.second] = EMPTY;

        return scoreA > scoreB; // 降序排列，优先高分位置
    });

    // 5. 使用minimax搜索最优解（深度提升至5层）
    int MAX_DEPTH = (moveCount_ < 10) ? 5 : 3; // 原3层提升至5层
    int bestScore = std::numeric_limits<int>::min();
    std::pair<int, int> bestMove = {-1, -1};

    for (const auto& [i, j] : candidates) {
        board_[i][j] = AI_PLAYER;
        int score = minimax(board_, MAX_DEPTH - 1, false, 
                           std::numeric_limits<int>::min(), 
                           std::numeric_limits<int>::max());
        board_[i][j] = EMPTY;

        if (score > bestScore) {
            bestScore = score;
            bestMove = {i, j};
        }
    }

    // 6. 若候选位置为空，随机选择空位（兜底逻辑）
    if (bestMove.first == -1) {
        std::vector<std::pair<int, int>> emptyCells;
        for (int i = 0; i < BOARD_SIZE; ++i) {
            for (int j = 0; j < BOARD_SIZE; ++j) {
                if (board_[i][j] == EMPTY) emptyCells.emplace_back(i, j);
            }
        }
        if (!emptyCells.empty()) {
            bestMove = emptyCells[rand() % emptyCells.size()];
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
// 新增：棋型类型枚举（活棋/冲棋/眠棋等）
enum PatternType {
    LIVE_ONE,    // 活一（两端空）
    LIVE_TWO,    // 活二（两端空）
    LIVE_THREE,  // 活三（两端空）
    LIVE_FOUR,   // 活四（两端空）
    BLOCKED_ONE, // 冲一（一端被堵）
    BLOCKED_TWO, // 冲二（一端被堵）
    BLOCKED_THREE,// 冲三（一端被堵）
    BLOCKED_FOUR, // 冲四（一端被堵）
    FIVE         // 五子连珠（胜利）
};

// 新增：棋型评分表（根据威胁程度调整）
const std::unordered_map<PatternType, int> PATTERN_SCORE = {
    {LIVE_ONE, 10},
    {LIVE_TWO, 100},
    {LIVE_THREE, 1000},
    {LIVE_FOUR, 100000},
    {BLOCKED_TWO, 50},
    {BLOCKED_THREE, 500},
    {BLOCKED_FOUR, 50000},
    {FIVE, 1000000} // 直接胜利的分数
};

// 优化后的评估函数：区分活棋/冲棋类型
int AiGame::evaluateBoard(const std::vector<std::vector<std::string>> &board, const std::string &player) {
    int score = 0;
    const int dx[] = {1, 0, 1, 1}; // 水平、垂直、右下、左下
    const int dy[] = {0, 1, 1, -1};

    for (int x = 0; x < BOARD_SIZE; ++x) {
        for (int y = 0; y < BOARD_SIZE; ++y) {
            if (board[x][y] != player) continue;

            for (int dir = 0; dir < 4; ++dir) {
                int count = 1; // 当前连续棋子数
                int leftBlock = 0, rightBlock = 0; // 左右是否被堵

                // 向左（反方向）扩展
                for (int i = 1; i < 5; ++i) {
                    int nx = x - i * dx[dir];
                    int ny = y - i * dy[dir];
                    if (!isInBoard(nx, ny) || board[nx][ny] == (player == HUMAN_PLAYER ? AI_PLAYER : HUMAN_PLAYER)) {
                        leftBlock = 1; // 被对手堵住或出界
                        break;
                    } else if (board[nx][ny] == EMPTY) break; // 遇到空位停止
                    count++;
                }

                // 向右（正方向）扩展
                for (int i = 1; i < 5; ++i) {
                    int nx = x + i * dx[dir];
                    int ny = y + i * dy[dir];
                    if (!isInBoard(nx, ny) || board[nx][ny] == (player == HUMAN_PLAYER ? AI_PLAYER : HUMAN_PLAYER)) {
                        rightBlock = 1; // 被对手堵住或出界
                        break;
                    } else if (board[nx][ny] == EMPTY) break; // 遇到空位停止
                    count++;
                }

                // 根据连续数和阻塞情况判断棋型
                PatternType type;
                if (count >= 5) {
                    type = FIVE;
                } else if (count == 4) {
                    if (leftBlock == 0 && rightBlock == 0) type = LIVE_FOUR;
                    else type = BLOCKED_FOUR;
                } else if (count == 3) {
                    if (leftBlock == 0 && rightBlock == 0) type = LIVE_THREE;
                    else type = BLOCKED_THREE;
                } else if (count == 2) {
                    if (leftBlock == 0 && rightBlock == 0) type = LIVE_TWO;
                    else type = BLOCKED_TWO;
                } else {
                    type = LIVE_ONE;
                }

                score += PATTERN_SCORE.at(type);
                
            }
        }
    }
    return score;
}

int AiGame::minimax(std::vector<std::vector<std::string>>& board, int depth, bool maximizing, int alpha, int beta) {
    // 提前终止条件：游戏结束或深度为0
    if (depth == 0 || isGameOver() || isDraw()) {
        return evaluateBoard(board, AI_PLAYER) - evaluateBoard(board, HUMAN_PLAYER);
    }

    int best = maximizing ? std::numeric_limits<int>::min() : std::numeric_limits<int>::max();
    std::string currentPlayer = maximizing ? AI_PLAYER : HUMAN_PLAYER;

    // 生成候选位置（仅靠近已有棋子的空位，减少搜索量）
    std::vector<std::pair<int, int>> candidates;
    for (int i = 0; i < BOARD_SIZE; ++i) {
        for (int j = 0; j < BOARD_SIZE; ++j) {
            if (board[i][j] == EMPTY && isNearOccupied(i, j)) {
                candidates.emplace_back(i, j);
            }
        }
    }

    // 对候选位置按评估分排序（提升剪枝效率）
    std::sort(candidates.begin(), candidates.end(), [this, &board, currentPlayer,maximizing](const auto& a, const auto& b) {
        board[a.first][a.second] = currentPlayer;
        int scoreA = evaluateBoard(board, currentPlayer);
        board[a.first][a.second] = EMPTY;

        board[b.first][b.second] = currentPlayer;
        int scoreB = evaluateBoard(board, currentPlayer);
        board[b.first][b.second] = EMPTY;

        return maximizing ? (scoreA > scoreB) : (scoreA < scoreB);
    });

    for (const auto& [i, j] : candidates) {
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

        if (beta <= alpha) break; // 剪枝
    }

    return best;
}

