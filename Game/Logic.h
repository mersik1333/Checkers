#pragma once
#include <random>
#include <vector>

#include "../Models/Move.h"
#include "Board.h"
#include "Config.h"

const int INF = 1e9;

class Logic
{
  public:
    Logic(Board *board, Config *config) : board(board), config(config)
    {
        optimization = (*config)("Bot", "Optimization");
    }

    vector<move_pos> find_best_turns(const bool color) {
        next_move.clear();
        next_best_state.clear();

        find_first_best_turn(board->get_board(), color, -1, -1, 0);

        vector<move_pos> res;
        int state = 0;
        do
        {
           res.push_back(next_move[state]);
           state = next_best_state[state];
        } 
        while (state != -1 && next_move[state].x != -1);
        return res;
    }

   private:
    double find_first_best_turn(vector<vector<POS_T>> mtx, const bool color, const POS_T x, const POS_T y, size_t state,
        double alpha = -1) {
        next_move.emplace_back(-1, -1, -1, -1);
        next_best_state.push_back(-1);
        if (state !=0)
        {
            find_turns(x, y, mtx);
        }
        auto now_turns = turns;
        auto now_have_beats = have_beats;

        if (!now_have_beats && state != 0)
        {
            return find_best_turns_rec(mtx, 1 - color, 0, alpha);
        }
        double best_score = -1;
        for (auto turn : now_turns) {
            size_t new_state = next_move.size();
            double score;
            if (now_have_beats) {
                score =  find_first_best_turn(make_turn(mtx, turn), color, turn.x2, turn.y2, new_state, best_score);
            }
            else {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, 0, best_score);
            }
            if (score > best_score) {
                best_score = score;
                next_move[state] = turn;
                next_best_state[state] = (now_have_beats ? new_state : -1);
            }
        }
        return best_score;
    }

    double find_best_turns_rec(vector<vector<POS_T>> mtx, const bool color, const size_t depth, double alpha = -1,
        double beta = INF + 1, const POS_T x = -1, const POS_T y = -1) {
        if (depth == Max_depth) {
            return calc_score(mtx, (depth % 2 == color));
        }
        if (x != -1) {
            find_turns(x, y, mtx);
        }
        else
        {
            find_turns(color, mtx);
        }
        auto now_turns = turns;
        auto now_have_beats = have_beats;
        if (!now_have_beats && x != -1) {
            return find_best_turns_rec(mtx, 1 - color, depth + 1, alpha, beta);
        }

        if (turns.empty()) {
            return (depth % 2 ? 0 : INF);
        }

        double min_score = INF + 1;
        double max_score = - 1;
        for (auto turn : now_turns) {
            double score;
            if (now_have_beats) {
                score = find_best_turns_rec(make_turn(mtx, turn), color, depth, alpha, beta, turn.x2, turn.y2);
            }
            else
            {
                score = find_best_turns_rec(make_turn(mtx, turn), 1 - color, depth + 1, alpha, beta);
            }
            min_score = min(min_score, score);
            max_score = max(max_score, score);

            if (depth % 2)
            {
                alpha = max(alpha, max_score);
            }
            else {
                beta = min(beta, min_score);
            }
            if (optimization != "O0" && alpha > beta) {
                break;
            }
            if (optimization == "O2" && alpha == beta) {
                return (depth % 2 ? max_score +1 : min_score - 1);
            }
        }
        return (depth % 2 ? max_score : min_score);
    }

    // Оценивает текущее состояние доски для бота. 
    // Параметры: 
    // mtx — матрица доски. 
    // first_bot_color — цвет бота (true = белые, false = чёрные). /// 
    // Алгоритм: 
    // 1. Подсчитываем количество обычных и дамочных шашек каждого цвета. 
    // 2. Если включён режим NumberAndPotential — добавляем небольшой бонус за продвижение обычных шашек вперёд. 
    // 3. Если бот играет чёрными — меняем местами счётчики, чтобы "w" всегда означало фигуры бота. 
    // 4. Если у бота нет фигур — возвращаем INF (проигрыш). 
    // 5. Если у соперника нет фигур — возвращаем 0 (победа). 
    // 6. Возвращаем отношение силы соперника к силе бота: (b + bq * q_coef) / (w + wq * q_coef)  Чем меньше значение — тем лучше позиция для бота.

    double calc_score(const vector<vector<POS_T>>& mtx, const bool first_bot_color) const
    {
        // color - who is max player
        double w = 0, wq = 0, b = 0, bq = 0;

        // Подсчёт фигур
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                w += (mtx[i][j] == 1);   // белые
                wq += (mtx[i][j] == 3);  // белые дамки
                b += (mtx[i][j] == 2);   // чёрные
                bq += (mtx[i][j] == 4);  // чёрные дамки
            }
        }
        // Если бот играет чёрными — меняем местами значения, 
        // чтобы "w" всегда означало фигуры бота.
        if (!first_bot_color)
        {
            swap(b, w);
            swap(bq, wq);
        }
        // Если у белых нет фигур — проигрыш
        if (w + wq == 0)
            return INF;
        // Если у черных нет фигур — победа
        if (b + bq == 0)
            return 0;
        // Коэффициент для дамок
        int q_coef = 4;
        // Возвращает отношение силы соперника к силе бота
        return (b + bq * q_coef) / (w + wq * q_coef);
    }

    // Выполняет ход на копии матрицы доски. 
        // Алгоритм: 
        // 1. Если ход является ударом (xb != -1) — снимаем побитую шашку. 
        // 2. Проверяем, превращается ли шашка в дамку:
        // - белая (1) становится дамкой (3), если дошла до 0-й строки. 
        // - чёрная (2) становится дамкой (4), если дошла до 7-й строки. 
        // 3. Перемещаем шашку на новую позицию. 
        // 4. Освобождаем старую клетку. 
        // Возвращает: новую матрицу после хода.
    vector<vector<POS_T>> make_turn(vector<vector<POS_T>> mtx, move_pos turn) const
    {
        // Если xb != -1 — это удар, удаляем побитую шашку
        if (turn.xb != -1)
            mtx[turn.xb][turn.yb] = 0;
        // Проверка превращения в дамку 
        // 1 → 3 (белая → белая дамка), если дошла до верхней строки 
        // 2 → 4 (чёрная → чёрная дамка), если дошла до нижней строки
        if ((mtx[turn.x][turn.y] == 1 && turn.x2 == 0) || (mtx[turn.x][turn.y] == 2 && turn.x2 == 7))
            mtx[turn.x][turn.y] += 2;
        // Перемещаем шашку на новую клетку
        mtx[turn.x2][turn.y2] = mtx[turn.x][turn.y];
        // Освобождаем старую клетку
        mtx[turn.x][turn.y] = 0;
        return mtx;
    }

public:
    // Находит все возможные ходы для заданного цвета. 
    // color Цвет игрока (0 или 1), для которого нужно найти ходы. 
    // Функция вызывает перегруженную версию find_turns, передавая текущее состояние доски из объекта Board. 

    void find_turns(const bool color)
    {
        find_turns(color, board->get_board());
    }

    // @brief Находит все возможные ходы для конкретной шашки по координатам X, Y
    // Функция вызывает find_turns, используя текущее состояние доски. 

    void find_turns(const POS_T x, const POS_T y)
    {
        find_turns(x, y, board->get_board());
    }

    // Находит все возможные ходы для всех шашек указанного цвета.
    // Алгоритм: 
    // 1. Обходит все клетки доски. 
    // 2. Для каждой шашки нужного цвета вызывает find_turns.
    // 3. Если хотя бы одна шашка может бить — сохраняются только бьющие ходы. 
    // 4. Если бить нельзя — сохраняются обычные ходы.
    // 5. Ходы перемешиваются для рандомизации поведения бота. 
    // Результат: список всех возможных ходов. 

private:
    void find_turns(const bool color, const vector<vector<POS_T>> &mtx)
    {
        vector<move_pos> res_turns;
        bool have_beats_before = false;
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (mtx[i][j] && mtx[i][j] % 2 != color)
                {
                    find_turns(i, j, mtx);
                    if (have_beats && !have_beats_before)
                    {
                        have_beats_before = true;
                        res_turns.clear();
                    }
                    if ((have_beats_before && have_beats) || !have_beats_before)
                    {
                        res_turns.insert(res_turns.end(), turns.begin(), turns.end());
                    }
                }
            }
        }
        turns = res_turns;
        have_beats = have_beats_before;
    }

    // Находит все возможные ходы для одной конкретной шашки
    // Алгоритм: 
    // 1. Определяет тип фигуры (обычная шашка или дамка). 
    // 2. Сначала ищет ВСЕ возможные удары: 
    // - Для обычных шашек: проверяет клетки через одну. 
    // - Для дамок: ищет удар на любой дистанции по диагонали. 
    // 3. Если удары найдены — обычные ходы НЕ рассматриваются. 
    // 4. Если ударов нет — ищет обычные ходы: 
    // - Для обычных шашек: один шаг вперёд по диагонали. 
    // - Для дамок: любое количество клеток по диагонали.
    // Результат: 
    // - turns — список всех ходов этой шашки. 
    // - have_beats — true, если найден хотя бы один удар.

    void find_turns(const POS_T x, const POS_T y, const vector<vector<POS_T>> &mtx)
    {
        turns.clear();
        have_beats = false;
        POS_T type = mtx[x][y];
        // check beats
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            for (POS_T i = x - 2; i <= x + 2; i += 4)
            {
                for (POS_T j = y - 2; j <= y + 2; j += 4)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7)
                        continue;
                    POS_T xb = (x + i) / 2, yb = (y + j) / 2;
                    if (mtx[i][j] || !mtx[xb][yb] || mtx[xb][yb] % 2 == type % 2)
                        continue;
                    turns.emplace_back(x, y, i, j, xb, yb);
                }
            }
            break;
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    POS_T xb = -1, yb = -1;
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                        {
                            if (mtx[i2][j2] % 2 == type % 2 || (mtx[i2][j2] % 2 != type % 2 && xb != -1))
                            {
                                break;
                            }
                            xb = i2;
                            yb = j2;
                        }
                        if (xb != -1 && xb != i2)
                        {
                            turns.emplace_back(x, y, i2, j2, xb, yb);
                        }
                    }
                }
            }
            break;
        }
        // check other turns
        if (!turns.empty())
        {
            have_beats = true;
            return;
        }
        switch (type)
        {
        case 1:
        case 2:
            // check pieces
            {
                POS_T i = ((type % 2) ? x - 1 : x + 1);
                for (POS_T j = y - 1; j <= y + 1; j += 2)
                {
                    if (i < 0 || i > 7 || j < 0 || j > 7 || mtx[i][j])
                        continue;
                    turns.emplace_back(x, y, i, j);
                }
                break;
            }
        default:
            // check queens
            for (POS_T i = -1; i <= 1; i += 2)
            {
                for (POS_T j = -1; j <= 1; j += 2)
                {
                    for (POS_T i2 = x + i, j2 = y + j; i2 != 8 && j2 != 8 && i2 != -1 && j2 != -1; i2 += i, j2 += j)
                    {
                        if (mtx[i2][j2])
                            break;
                        turns.emplace_back(x, y, i2, j2);
                    }
                }
            }
            break;
        }
    }

  public:
      // Список всех возможных ходов, найденных последним вызовом find_turns(). 
      // Заполняется как для одной шашки, так и для всего цвета.
      vector<move_pos> turns;

      // Флаг, показывающий, есть ли среди найденных ходов хотя бы один удар. 
      // Если true — обычные ходы игнорируются.
      bool have_beats;

      // Максимальная глубина рекурсивного поиска (Minimax). 
      // Определяет «силу» бота: чем больше глубина, тем сильнее игра.
      int Max_depth;

  private:
      // Режим оптимизации поиска. // Например: "O0", "O1", "O2" — влияет на включение alpha-beta отсечение.
      string optimization;
      // Массив, где для каждого состояния Minimax хранится выбранный ход. 
      // Используется для восстановления цепочки ходов (например, серии ударов).
      vector<move_pos> next_move;
      // Массив, где для каждого состояния хранится индекс следующего состояния. 
      // Позволяет восстановить последовательность ходов, ведущую к лучшему результату.
      vector<int> next_best_state;
      // Указатель на объект доски. 
      // Используется для получения текущего состояния игрового поля.
      Board* board;
      // Указатель на объект конфигурации. 
      // Содержит настройки бота: глубина поиска, режим оценки, рандомизация и т.д.
      Config* config;
};
