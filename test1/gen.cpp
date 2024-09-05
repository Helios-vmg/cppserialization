#include "gen.hpp"

#ifndef NONUNIFORM_SAMPLING

void gen(std::uint8_t &dst, std::mt19937 &rng){
	typedef std::uint8_t T;
	std::uniform_int_distribution<int> d(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	dst = (T)d(rng);
}

void gen(std::int8_t &dst, std::mt19937 &rng){
	typedef std::int8_t T;
	std::uniform_int_distribution<int> d(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	dst = (T)d(rng);
}

void gen(char &dst, std::mt19937 &rng){
	std::uint8_t temp;
	gen(temp, rng);
	dst = (char)temp;
}

void gen(char32_t &dst, std::mt19937 &rng){
	std::uint32_t temp;
	gen(temp, rng);
	dst = (char32_t)temp;
}

void gen(std::uint64_t &dst, std::mt19937 &rng){
	dst = ((std::uint64_t)rng() << 32) | (std::uint64_t)rng();
}

void gen(std::int64_t &dst, std::mt19937 &rng){
	dst = ((std::uint64_t)rng() << 32) | (std::uint64_t)rng();
}

#else


void gen(std::uint8_t &dst, std::mt19937 &rng){
	typedef std::uint8_t T;
	std::uniform_int_distribution<int> d(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	dst = (T)d(rng);
}

void gen(std::int8_t &dst, std::mt19937 &rng){
	typedef std::int8_t T;
	std::uniform_int_distribution<int> d(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	dst = (T)d(rng);
}

void gen(char &dst, std::mt19937 &rng){
	std::uint8_t temp;
	gen(temp, rng);
	dst = (char)temp;
}

void gen(char32_t &dst, std::mt19937 &rng){
	std::uint32_t temp;
	gen(temp, rng);
	dst = (char32_t)temp;
}

void gen(std::uint64_t &dst, std::mt19937 &rng){
	dst = ((std::uint64_t)rng() << 32) | (std::uint64_t)rng();
}

void gen(std::int64_t &dst, std::mt19937 &rng){
	dst = ((std::uint64_t)rng() << 32) | (std::uint64_t)rng();
}


#endif
