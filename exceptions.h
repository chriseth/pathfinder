#pragma once

#include <exception>

class Exception: public std::exception {};
class InvalidArgumentException: public std::exception {};

inline void require(bool _condition)
{
	if (!_condition)
		throw Exception();
}
