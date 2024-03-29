#pragma once

#include "math/arithmetic.hpp"

template<arithmetic A>
struct vector2
{
    union { A x, s, u, w; };
    union { A y, t, v, h; };
};

template <arithmetic A>
bool operator == (const vector2<A>& l, const vector2<A>& r)
{
    return l.x == r.x && l.y == r.y;
}


typedef vector2<int> vector2i;
typedef vector2<unsigned int> vector2ui;
typedef vector2<float> vector2f;
typedef vector2<double> vector2d;
