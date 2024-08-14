#pragma once

#include "noexcept.h"
#include <exception>
#include <string>

class ParsingError : public std::exception{
	std::string message;
public:
	ParsingError(){}
	template <typename T>
	ParsingError &operator<<(const T &o){
		std::stringstream stream;
		stream << o;
		this->message += stream.str();
		return *this;
	}
	const char *what() const NOEXCEPT override{
		return this->message.c_str();
	}
};
