#pragma once
#include <stdlib.h>

// Тип для координат клетки на доске (значения от 0 до 7)
typedef int8_t POS_T;

// Структура, описывающая один возможный ход в шашках
struct move_pos
{
    POS_T x, y;             // начальная клетка хода (координаты фигуры "откуда")
    POS_T x2, y2;           // конечная клетка хода (координаты "куда")
    POS_T xb = -1, yb = -1; // координаты побитой шашки; -1 означает, что взятия нет

    // Конструктор для обычного хода без взятия
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2)
        : x(x), y(y), x2(x2), y2(y2)
    {
    }

    // Конструктор для хода со взятием (указываются координаты побитой шашки)
    move_pos(const POS_T x, const POS_T y, const POS_T x2, const POS_T y2,
        const POS_T xb, const POS_T yb)
        : x(x), y(y), x2(x2), y2(y2), xb(xb), yb(yb)
    {
    }

    // Сравнение двух ходов: равны, если совпадают начальная и конечная клетки
    // (взятие не учитывается, т.к. оно определяется по позиции)
    bool operator==(const move_pos& other) const
    {
        return (x == other.x && y == other.y &&
            x2 == other.x2 && y2 == other.y2);
    }

    // Неравенство — логическое отрицание оператора ==
    bool operator!=(const move_pos& other) const
    {
        return !(*this == other);
    }
};