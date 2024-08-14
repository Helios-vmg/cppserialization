#include "util.h"

std::string serialize(const Serializable &src){
	std::stringstream temp;
	SerializerStream ss(temp);
	ss.full_serialization(src, true);
	return temp.str();
}

void test_assertion(bool check, const char *message){
	if (!check)
		throw std::runtime_error(message);
}
