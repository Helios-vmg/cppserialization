#pragma once

#include <random>
#include <optional>
#include <array>

#define NONUNIFORM_SAMPLING

#ifndef NONUNIFORM_SAMPLING

void gen(std::uint8_t &dst, std::mt19937 &rng);
void gen(std::int8_t &dst, std::mt19937 &rng);
void gen(char &dst, std::mt19937 &rng);
void gen(char32_t &dst, std::mt19937 &rng);
void gen(std::uint64_t &dst, std::mt19937 &rng);
void gen(std::int64_t &dst, std::mt19937 &rng);

template <typename T>
void gen(T &dst, std::mt19937 &rng){
	std::uniform_int_distribution<T> d(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	dst = d(rng);
}

template <typename T>
std::enable_if_t<std::is_integral_v<T> && (sizeof(T) > 1) && sizeof(T) <= sizeof(std::mt19937::result_type), void>
gen(T &dst, std::mt19937 &rng){
	std::uniform_int_distribution<T> d(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
	dst = d(rng);
}

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, void>
gen(T &dst, std::mt19937 &rng){
	std::uniform_real_distribution<T> d(0);
	dst = d(rng);
}

template <typename T>
void gen(std::basic_string<T> &dst, std::mt19937 &rng){
	std::uniform_int_distribution<unsigned> d(0, 1024);
	dst.resize(d(rng));
	for (auto &c : dst)
		gen(c, rng);
}

template <typename T>
void gen(std::optional<T> &dst, std::mt19937 &rng){
	dst.reset();
	if (rng() % 2 == 0){
		dst.emplace();
		gen(*dst, rng);
	}
}

#else

template <size_t N>
std::array<std::uint8_t, N> generate_bits(int bits, std::mt19937 &rng){
	std::array<std::uint8_t, N> ret;
	memset(ret.data(), 0, N);
	int j = 0;
	while (bits > 0){
		auto n = rng();
		for (int i = 0; i < 4 && bits > 0; i++){
			std::uint8_t mask = bits >= 8 ? 0xFF : ((1 << bits) - 1);
			ret[j] = n & mask;
			n >>= 8;
			bits -= 8;
		}
	}
	return ret;
}

template <typename T>
std::enable_if_t<std::is_integral_v<T> && std::is_signed_v<T>, void>
conditional_negation(T &dst, std::mt19937 &rng){
	if (rng() % 2)
		dst = -dst;
}

template <typename T>
std::enable_if_t<std::is_integral_v<T> && !std::is_signed_v<T>, void>
conditional_negation(T &dst, std::mt19937 &rng){}

template <typename T>
std::enable_if_t<std::is_integral_v<T>, void>
gen(T &dst, std::mt19937 &rng){
	constexpr auto N = sizeof(T);
	auto bits = std::uniform_int_distribution<int>(0, N * 8)(rng);
	auto buffer = generate_bits<N>(bits, rng);
	dst = 0;
	for (size_t i = 0; i < N; i++){
		dst <<= 8;
		dst |= buffer[N - 1 - i];
	}
	conditional_negation(dst, rng);
}

template <typename T>
std::enable_if_t<std::is_floating_point_v<T>, void>
gen(T &dst, std::mt19937 &rng){
	std::uniform_real_distribution<T> d(0);
	dst = d(rng);
}

template <typename T>
void gen(std::basic_string<T> &dst, std::mt19937 &rng){
	std::uniform_int_distribution<unsigned> d(0, 1024);
	dst.resize(d(rng));
	for (auto &c : dst)
		gen(c, rng);
}

template <typename T>
void gen(std::optional<T> &dst, std::mt19937 &rng){
	dst.reset();
	if (rng() % 2 == 0){
		dst.emplace();
		gen(*dst, rng);
	}
}


#endif
