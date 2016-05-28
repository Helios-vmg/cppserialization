#ifndef GENERIC_EXCEPTION
#define GENERIC_EXCEPTION

#include <exception>
#include "noexcept.h"

class GenericException : public std::exception{
	const char *cmessage;
public:
	GenericException(const char *message = ""): cmessage(message){}
	virtual ~GenericException(){}
	virtual const char *what() const NOEXCEPT{
		return this->cmessage;
	}
};


#endif
