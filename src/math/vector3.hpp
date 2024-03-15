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
    union { A x, r; };
    union { A y, g; };
	union { A z, b; };

    friend bool operator ==<> (const vector3<A>& l, const vector3<A>& r);
};

struct vector3i : public vector3<int> {};

struct vector3f : public vector3<float> {};

struct vector3d : public vector3<double> {};
