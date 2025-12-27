#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс, отвечающий за обработку действий игрока (мышь, закрытие окна и т.п.)
class Hand
{
public:
    Hand(Board* board) : board(board)
    {
    }

    // Ожидание выбора клетки игроком.
    // Возвращает: тип действия (Response) + координаты клетки (xc, yc)
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK; // текущее состояние ответа
        int x = -1, y = -1;           // координаты клика в пикселях
        int xc = -1, yc = -1;         // координаты клетки на доске

        while (true)
        {
            // Проверяем, произошло ли событие
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    // Игрок закрыл окно
                    resp = Response::QUIT;
                    break;

                case SDL_MOUSEBUTTONDOWN:
                    // Получаем координаты клика мыши
                    x = windowEvent.motion.x;
                    y = windowEvent.motion.y;

                    // Переводим пиксели → координаты клетки
                    // Доска занимает 8×8 клеток, но окно содержит рамки/панели
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Клик по кнопке "Назад"
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)
                    {
                        resp = Response::BACK;
                    }
                    // Клик по кнопке "Повторить игру"
                    else if (xc == -1 && yc == 8)
                    {
                        resp = Response::REPLAY;
                    }
                    // Клик по игровой клетке 8×8
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)
                    {
                        resp = Response::CELL;
                    }
                    else
                    {
                        // Клик вне допустимых зон
                        xc = -1;
                        yc = -1;
                    }
                    break;

                case SDL_WINDOWEVENT:
                    // Если окно было изменено по размеру — пересчитать размеры доски
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)
                    {
                        board->reset_window_size();
                        break;
                    }
                }

                // Если действие определено — выходим из цикла
                if (resp != Response::OK)
                    break;
            }
        }

        // Возвращаем тип действия + координаты клетки
        return { resp, xc, yc };
    }

    // Ожидание простого действия (например, на финальном экране)
    Response wait() const
    {
        SDL_Event windowEvent;
        Response resp = Response::OK;

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:
                    // Закрытие окна
                    resp = Response::QUIT;
                    break;

                case SDL_WINDOWEVENT_SIZE_CHANGED:
                    // Пересчёт размеров доски при изменении окна
                    board->reset_window_size();
                    break;

                case SDL_MOUSEBUTTONDOWN: {
                    // Проверяем, нажата ли кнопка "Повторить игру"
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);
                    int yc = int(x / (board->W / 10) - 1);

                    if (xc == -1 && yc == 8)
                        resp = Response::REPLAY;
                }
                                        break;
                }

                if (resp != Response::OK)
                    break;
            }
        }

        return resp;
    }

private:
    Board* board; // указатель на игровую доску (нужен для вычисления координат и пересчёта размеров)
};