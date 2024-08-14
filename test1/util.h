#pragma once

#include <Serializable.h>
#include <ExampleDeserializerStream.h>
#include <optional>
#include <sstream>

template <typename T>
bool operator==(const std::optional<T> &l, const std::optional<T> &r){
	if (!l != !r)
		return false;
	if (!l)
		return true;
	return *l == *r;
}

template <typename T>
bool operator!=(const std::optional<T> &l, const std::optional<T> &r){
	return !(l == r);
}

std::string serialize(const Serializable &src);
void test_assertion(bool check, const char *message);

template <typename T>
std::enable_if_t<std::is_base_of_v<Serializable, T>, std::unique_ptr<T>>
deserialize(const std::string &src){
	std::stringstream temp(src);
	ExampleDeserializerStream eds(temp);
	return eds.full_deserialization<T>(true);
}

template <typename T>
std::unique_ptr<T> round_trip(const T &src){
	return deserialize<T>(serialize(src));
}
