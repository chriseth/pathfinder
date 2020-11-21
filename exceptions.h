#pragma once

#include <exception>
#include <iostream>

class Exception: public std::exception {};
class InvalidArgumentException: public std::exception {};

#define require(condition) \
	do {\
		if (!bool(condition))\
		{\
			std::cerr << "Failing assumption: " << __FILE__ << ":" << __LINE__ << #condition << std::endl;\
			throw Exception();\
		}\
	} while (false)
