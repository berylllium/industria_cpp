#pragma once

#include "math/arithmetic.hpp"

// Pre-define friend-template functions so the compiler knows they're templated.
template <arithmetic A> struct vector3;
template <arithmetic A> bool operator == (const vector3<A>& l, const vector3<A>& r)
{
    return l.x == r.x && l.y == r.y && l.z == r.z;
}

template<arithmetic A>
struct vector3
{
    union { A x, r = 0; };
    union { A y, g = 0; };
    union { A z, b = 0; };

    friend bool operator ==<> (const vector3<A>& l, const vector3<A>& r);
};

template<arithmetic A>
vector3<A> operator - (const vector3<A>& l, const vector3<A>& r)
{
    return vector3<A> {
        l.x - r.x,
        l.y - r.y,
        l.z - r.z
    };
}

template<arithmetic A>
vector3<A> operator + (const vector3<A>& l, const vector3<A>& r)
{
    return vector3<A> {
        l.x + r.x,
        l.y + r.y,
        l.z + r.z
    };
}

template<arithmetic A>
vector3<A> operator * (A l, const vector3<A>& r)
{
    return vector3<A> {
        l * r.x,
        l * r.y,
        l * r.z
    };
}

template<arithmetic A>
vector3<A> operator / (const vector3<A>& l, A r)
{
    return vector3<A> {
        l.x / r,
        l.y / r,
        l.z / r
    };
}

template<arithmetic A>
vector3<A> operator % (const vector3<A>& l, A r)
{
    return vector3<A> {
        l.x % r,
        l.y % r,
        l.z % r
    };
}

typedef vector3<int> vector3i;
typedef vector3<float> vector3f;
typedef vector3<double> vector3d;
