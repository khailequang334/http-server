#pragma once
#include <string>

template <typename T>
std::string ToString(T value);

template <typename T>
T FromString(const std::string& str);

