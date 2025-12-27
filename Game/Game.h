#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

class Game
{
public:
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);
        fout.close();
    }

    // to start checkers
    int play()
    {
        // фиксируем время начала партии
        auto start = chrono::steady_clock::now();

        // если игрок выбрал "повторить игру", то сбрасываем состояние логики и конфигурации
        if (is_replay)
        {
            logic = Logic(&board, &config); // пересоздаём объект логики
            config.reload();                // перезагружаем настройки
            board.redraw();                 // перерисовываем доску
        }
        else
        {
            board.start_draw();             // начальная отрисовка доски
        }
        is_replay = false;

        int turn_num = -1;                  // номер хода (будет увеличен в цикле)
        bool is_quit = false;               // флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns"); // максимальное число ходов

        // основной игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;                // количество последовательных взятий
            logic.find_turns(turn_num % 2); // ищем доступные ходы для текущего игрока

            if (logic.turns.empty())        // если ходов нет — игра окончена
                break;

            // устанавливаем глубину поиска для бота в зависимости от цвета
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + "BotLevel");

            // если текущий игрок — человек
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + "Bot"))
            {
                auto resp = player_turn(turn_num % 2); // обработка хода игрока

                if (resp == Response::QUIT)            // игрок вышел
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)     // игрок запросил повтор
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)       // игрок отменил ход
                {
                    // если предыдущий ход был бота и не было взятия — откатываем два хода
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + "Bot") &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();
                        --turn_num;
                    }

                    // если не было взятия — откатываем ход игрока
                    if (!beat_series)
                        --turn_num;

                    board.rollback(); // откат хода
                    --turn_num;
                    beat_series = 0;
                }
            }
            else
            {
                // ход делает бот
                bot_turn(turn_num % 2);
            }
        }

        // фиксируем время окончания игры и записываем в лог
        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();

        // если был запрос на повтор — запускаем игру заново
        if (is_replay)
            return play();

        // если игрок вышел — возвращаем 0
        if (is_quit)
            return 0;

        // определяем результат игры
        int res = 2; // ничья по умолчанию
        if (turn_num == Max_turns)
        {
            res = 0; // ничья по лимиту ходов
        }
        else if (turn_num % 2)
        {
            res = 1; // победил белый или чёрный (зависит от логики)
        }

        // показываем финальный экран
        board.show_final(res);

        // ждём реакции игрока (например, повторить игру)
        auto resp = hand.wait();
        if (resp == Response::REPLAY)
        {
            is_replay = true;
            return play();
        }

        return res; // возвращаем результат партии
    }

private:
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();

        auto delay_ms = config("Bot", "BotDelayMS");
        // new thread for equal delay for each turn
        thread th(SDL_Delay, delay_ms);
        auto turns = logic.find_best_turns(color);
        th.join();
        bool is_first = true;
        // making moves
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);
            }
            is_first = false;
            beat_series += (turn.xb != -1);
            board.move_piece(turn, beat_series);
        }

        auto end = chrono::steady_clock::now();
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    Response player_turn(const bool color)
    {
        // Формируем список клеток, с которых игрок может начать ход
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);
        }

        // Подсвечиваем клетки, доступные для выбора фигуры
        board.highlight_cells(cells);

        // pos — выбранный игроком ход (пока пустой)
        move_pos pos = { -1, -1, -1, -1 };

        // x, y — координаты выбранной фигуры (первый клик)
        POS_T x = -1, y = -1;

        // --- ЭТАП 1: выбор фигуры и конечной клетки для первого хода ---
        while (true)
        {
            // Получаем действие игрока: либо выбор клетки, либо BACK/QUIT/REPLAY
            auto resp = hand.get_cell();

            // Если игрок нажал не на клетку — возвращаем соответствующий Response
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);

            // Координаты выбранной клетки
            pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

            bool is_correct = false;

            // Проверяем: является ли клетка начальной точкой возможного хода
            for (auto turn : logic.turns)
            {
                // Если игрок выбрал фигуру, с которой можно ходить
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }

                // Если игрок уже выбрал фигуру, и теперь выбирает конечную клетку
                if (turn == move_pos{ x, y, cell.first, cell.second })
                {
                    pos = turn; // найден корректный ход
                    break;
                }
            }

            // Если найден полный ход — выходим из цикла
            if (pos.x != -1)
                break;

            // Если выбор некорректен
            if (!is_correct)
            {
                // Если ранее была выбрана фигура — сбрасываем подсветку
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells); // подсвечиваем начальные клетки заново
                }
                x = -1;
                y = -1;
                continue;
            }

            // Игрок выбрал корректную фигуру — запоминаем её координаты
            x = cell.first;
            y = cell.second;

            // Обновляем подсветку: показываем возможные ходы этой фигуры
            board.clear_highlight();
            board.set_active(x, y);

            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);
                }
            }

            board.highlight_cells(cells2);
        }

        // --- ЭТАП 2: выполнение первого хода ---
        board.clear_highlight();
        board.clear_active();

        // Делаем ход. Если xb != -1 — это взятие
        board.move_piece(pos, pos.xb != -1);

        // Если не было взятия — ход завершён
        if (pos.xb == -1)
            return Response::OK;

        // --- ЭТАП 3: серия обязательных взятий ---
        beat_series = 1;

        while (true)
        {
            // Ищем возможные продолжения взятия с новой позиции
            logic.find_turns(pos.x2, pos.y2);

            // Если больше нет взятий — серия закончена
            if (!logic.have_beats)
                break;

            // Подсвечиваем клетки, куда можно продолжить бить
            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);
            }

            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);

            // Ждём, пока игрок выберет корректное продолжение взятия
            while (true)
            {
                auto resp = hand.get_cell();

                // Если игрок нажал не на клетку — возвращаем действие (QUIT/BACK/REPLAY)
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);

                pair<POS_T, POS_T> cell{ get<1>(resp), get<2>(resp) };

                bool is_correct = false;

                // Проверяем, является ли выбранная клетка корректным продолжением взятия
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }

                if (!is_correct)
                    continue; // ждём корректный выбор

                // Делаем очередное взятие
                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);
                break;
            }
        }

        // Все обязательные взятия выполнены
        return Response::OK;
    }


private:
    Config config;
    Board board;
    Hand hand;
    Logic logic;
    int beat_series;
    bool is_replay = false;
};