#ifndef EASYBIGNUM_H
#define EASYBIGNUM_H

#include <cstdint>
#include <random>
#include <iostream>
#include <string>
#include <vector>
#include <limits>
#include <type_traits>

class EasyBigNum{
	typedef uintptr_t T;
	std::vector<T> data;
	T div_aux;
	unsigned div_shift_amount;
	static const T max = std::numeric_limits<T>::max();
	static const T bits = sizeof(T) * 8;
	bool overflow;
	void reduce();
	static T multiplication_carry(T dst, const T src);
	std::vector<char> prepare_exponent() const;
	void div_shift(T bit);
	bool div_geq(const EasyBigNum &other) const;
	void div_shift_normalize();
	bool is_prime_trial_division(const std::vector<bool> &sieve) const;
	template <typename Random>
	bool is_prime_fermat(const unsigned k, Random &source) const{
		auto &n = *this;
		auto n_minus_1 = n - 1;
		auto n_minus_2 = n_minus_1 - 1;
		for (auto i = k; i--;){
			EasyBigNum pick(source, n_minus_2, 2);
			if (pick.mod_pow(n_minus_1, n) != 1)
				return false;
		}
		return true;
	}
	template <typename Random>
	bool is_prime_miller_rabin(const unsigned k, Random &source) const{
		auto &n = *this;
		auto d = n - 1;
		unsigned r = 0;
		while (d.even()){
			d >>= 1;
			r++;
		}
		const auto n_minus_1 = n - 1;
		const auto n_minus_2 = n_minus_1 - 1;
		for (auto i = k; i--;){
			EasyBigNum pick(source, n_minus_2, 2);
			pick = pick.mod_pow(d, n);
			if (pick == 1 || pick == n_minus_1)
				continue;
			bool done = true;
			for (auto j = r - 1; j--;){
				pick = pick.mod_pow(2, n);
				if (pick == 1)
					return false;
				if (pick == n_minus_1){
					done = false;
					break;
				}
			}
			if (done)
				return false;
		}
		return true;
	}

public:
	EasyBigNum() : data(1, 0), overflow(false){}
	template <typename T2>
	EasyBigNum(T2 value, typename std::enable_if<std::is_integral<T2>::value, T2>::type * = nullptr): data(1, (T)value), overflow(false){}
	EasyBigNum(const EasyBigNum &other): data(other.data), overflow(other.overflow){}
	EasyBigNum(EasyBigNum &&other)
		: data(std::move(other.data))
		, overflow(other.overflow)
	{
		other.data.push_back(0);
		other.overflow = false;
	}
	EasyBigNum(const char *value);
	template <typename Randomness>
	EasyBigNum(Randomness &source, EasyBigNum max, const EasyBigNum &min = 0){
		this->overflow = false;
		std::uniform_int_distribution<T> dist;
		max -= min;
		auto rand_max = max;
		auto more_max = max + 1;
		rand_max.all_bits_on();
		rand_max = rand_max - (rand_max + 1) % more_max;

		do{
			this->data.resize(max.data.size());
			for (auto &i : this->data)
				i = dist(source);
			this->reduce();
		}while (*this > rand_max);
		*this %= more_max;
		*this += min;
	}
	// ((const unsigned char *)buffer)[0] -> least significant byte
	// ((const unsigned char *)buffer)[size - 1] -> most significant byte
	EasyBigNum(const void *buffer, size_t size);
	// ((const unsigned char *)buffer)[0] -> least significant byte
	// ((const unsigned char *)buffer)[size - 1] -> most significant byte
	template <typename T2>
	EasyBigNum(const std::vector<T2> &buffer, typename std::enable_if<std::is_integral<T2>::value && sizeof(T2) == 1>::type * = nullptr)
		: EasyBigNum(buffer.size() ? &buffer[0] : nullptr, buffer.size()){}
	void all_bits_on();
	const EasyBigNum &operator=(const EasyBigNum &other);
	const EasyBigNum &operator=(const EasyBigNum &&other);
	const EasyBigNum &operator+=(const EasyBigNum &other);
	const EasyBigNum &operator-=(const EasyBigNum &other);
	const EasyBigNum &operator<<=(T shift);
	const EasyBigNum &operator>>=(T shift);
	EasyBigNum operator*(const EasyBigNum &other) const;
	bool even() const{
		return this->data.front() % 2 == 0;
	}
	bool odd() const{
		return !this->even();
	}
	EasyBigNum pow(const EasyBigNum &exponent) const;
	EasyBigNum mod_pow(const EasyBigNum &exponent, const EasyBigNum &modulo) const;
	// first = quotient, second = remainder
	std::pair<EasyBigNum, EasyBigNum> div(const EasyBigNum &other) const;
	EasyBigNum operator/(const EasyBigNum &other) const;
	EasyBigNum operator%(const EasyBigNum &other) const;
	EasyBigNum operator+(const EasyBigNum &other) const{
		auto ret = *this;
		ret += other;
		return ret;
	}
	EasyBigNum operator-(const EasyBigNum &other) const{
		auto ret = *this;
		ret -= other;
		return ret;
	}
	EasyBigNum operator<<(T other) const{
		auto ret = *this;
		ret <<= other;
		return ret;
	}
	EasyBigNum operator >> (T other) const{
		auto ret = *this;
		ret >>= other;
		return ret;
	}
	const EasyBigNum &operator*=(const EasyBigNum &other){
		*this = *this * other;
		return *this;
	}
	const EasyBigNum &operator/=(const EasyBigNum &other){
		*this = *this / other;
		return *this;
	}
	const EasyBigNum &operator%=(const EasyBigNum &other){
		*this = *this % other;
		return *this;
	}
	bool operator!() const{
		for (auto i : this->data)
			if (i)
				return false;
		return true;
	}
	bool operator==(const EasyBigNum &other) const;
	bool operator!=(const EasyBigNum &other) const{
		return !(*this == other);
	}
	bool operator<(const EasyBigNum &other) const;
	bool operator>(const EasyBigNum &other) const{
		return other < *this;
	}
	bool operator<=(const EasyBigNum &other) const{
		return !(*this > other);
	}
	bool operator>=(const EasyBigNum &other) const{
		return !(*this < other);
	}
	std::string to_string() const;
	std::string to_string_hex() const;

	struct primality_config{
		unsigned sieve_size = 1 << 10;
		unsigned fermat_tests = 10;
		unsigned miller_rabin_tests = 100;
	};

	static std::vector<bool> create_sieve(unsigned max);
	
	template <typename Random>
	bool is_probably_prime(const std::vector<bool> &sieve, Random &source, const primality_config &config = primality_config()) const{
		auto &n = *this;
		if (!n.is_prime_trial_division(sieve))
			return false;
		if (!n.is_prime_fermat(config.fermat_tests, source))
			return false;
		if (!n.is_prime_miller_rabin(config.miller_rabin_tests, source))
			return false;
		return true;
	}

	template <typename Random>
	static EasyBigNum generate_prime(Random &source, const EasyBigNum &bits, primality_config config = primality_config()){
		auto sieve = create_sieve(config.sieve_size);
		auto ret = EasyBigNum(source, EasyBigNum(2).pow(bits) - 1);
		if (ret.even())
			ret += 1;
		while (!ret.is_probably_prime(sieve, source, config))
			ret -= 2;
		return ret;
	}

	EasyBigNum gcd(EasyBigNum b) const;
	std::vector<unsigned char> to_buffer() const;
	size_t all_bits() const{
		return this->data.size() * this->bits;
	}
	size_t active_bits() const {
		auto ret = (this->data.size() - 1) * this->bits;
		for (auto word = this->data.back(); word; word >>= 1)
			ret++;
		return ret;
	}
};

std::ostream &operator<<(std::ostream &stream, const EasyBigNum &n);

class EasySignedBigNum{
	EasyBigNum bignum;
	bool sign;
	EasySignedBigNum sum(const EasySignedBigNum &other, bool flip_right_sign) const;
public:
	EasySignedBigNum() : bignum(), sign(false) {}
	EasySignedBigNum(int value) : bignum(value), sign(value < 0){}
	EasySignedBigNum(const EasyBigNum &b): bignum(b), sign(false){}
	EasySignedBigNum(EasyBigNum &&b): bignum(std::move(b)), sign(false){}
	EasySignedBigNum(const char *s): EasySignedBigNum(){
		if (!s || !*s)
			return;
		for (; *s == '-'; s++)
			this->sign = !this->sign;
		this->bignum = EasyBigNum(s);
	}
	bool positive() const{
		return !this->sign;
	}
	bool negative() const{
		return this->sign;
	}
	void invert_sign(){
		this->sign = !this->sign;
	}
	EasySignedBigNum operator-() const{
		auto ret = *this;
		ret.invert_sign();
		return ret;
	}
	const EasyBigNum &make_positive() const{
		return this->bignum;
	}
	bool operator==(const EasySignedBigNum &other) const {
		return this->bignum == other.bignum && this->sign == other.sign;
	}
	bool operator!=(const EasySignedBigNum &other) const {
		return !(*this == other);
	}
	bool operator<(const EasySignedBigNum &other) const;
	bool operator>(const EasySignedBigNum &other) const {
		return other < *this;
	}
	bool operator<=(const EasySignedBigNum &other) const {
		return !(*this > other);
	}
	bool operator>=(const EasySignedBigNum &other) const {
		return !(*this < other);
	}
	EasySignedBigNum operator+(const EasySignedBigNum &other) const;
	EasySignedBigNum operator-(const EasySignedBigNum &other) const;
	const EasySignedBigNum &operator*=(const EasySignedBigNum &other){
		this->bignum *= other.bignum;
		this->sign ^= other.sign;
		return *this;
	}
	const EasySignedBigNum &operator/=(const EasySignedBigNum &other){
		this->bignum /= other.bignum;
		this->sign ^= other.sign;
		return *this;
	}
	const EasySignedBigNum &operator%=(const EasySignedBigNum &other){
		this->bignum %= other.bignum;
		this->sign = this->sign;
		return *this;
	}
	std::pair<EasySignedBigNum, EasySignedBigNum> div(const EasySignedBigNum &other) const{
		auto temp = this->bignum.div(other.bignum);
		std::pair<EasySignedBigNum, EasySignedBigNum> ret;
		ret.first.bignum = std::move(temp.first);
		ret.second.bignum = std::move(temp.second);
		ret.first.sign = this->sign ^ other.sign;
		ret.second.sign = this->sign;
		return ret;
	}
	const EasySignedBigNum &operator+=(const EasySignedBigNum &other){
		return *this = *this + other;
	}
	const EasySignedBigNum &operator-=(const EasySignedBigNum &other){
		return *this = *this - other;
	}
	EasySignedBigNum operator*(const EasySignedBigNum &other) const{
		auto ret = *this;
		ret *= other;
		return ret;
	}
	EasySignedBigNum operator/(const EasySignedBigNum &other) const {
		auto ret = *this;
		ret /= other;
		return ret;
	}
	EasySignedBigNum operator%(const EasySignedBigNum &other) const {
		auto ret = *this;
		ret %= other;
		return ret;
	}
	bool operator!() const{
		return !this->bignum;
	}
	std::string to_string() const{
		std::string ret;
		if (this->sign)
			ret += '-';
		ret += this->bignum.to_string();
		return ret;
	}
};

std::ostream &operator<<(std::ostream &stream, const EasySignedBigNum &n);

#endif
