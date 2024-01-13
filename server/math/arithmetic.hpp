#pragma once

#include <type_traits>
#include <concepts>

template<typename T>
concept arithmetic = std::is_arithmetic_v<T>;
