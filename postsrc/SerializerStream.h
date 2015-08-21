#ifndef SERIALIZERSTREAM_H
#define SERIALIZERSTREAM_H

#include <iostream>
#include <cstdint>
#include <cmath>
#include <type_traits>
#include <vector>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <boost/static_assert.hpp>
#include <stack>
#include "serialization_utils.h"
#include "Serializable.h"

template <typename T>
typename std::make_unsigned<T>::type ints_to_uints(T z){
	typedef typename std::make_unsigned<T>::type u;
	if (z >= 0)
		return (u)z * 2;
	return (u)-(z + 1) * 2 + 1;
}

template <typename T>
typename std::make_signed<T>::type uints_to_ints(T n){
	typedef typename std::make_signed<T>::type s;
	if ((n & 1) == 0)
		return (s)(n / 2);
	return -(s)((n - 1) / 2) - 1;
}

template <typename FP, typename Out>
void to_ints(Out &int_mantissa, int &exponent, FP x){
	auto mantissa = frexp(x, &exponent);
	// mantissa in [0.5; 1)
	mantissa = (mantissa - 0.5) * 2;
	// mantissa in [0; 1)
	int_mantissa = (Out)(mantissa * (FP)std::numeric_limits<FP>::max());
}

template <typename FP>
struct floating_point_mapping{};

template <>
struct floating_point_mapping<float>{
	typedef std::uint32_t type;
};

template <>
struct floating_point_mapping<double>{
	typedef std::uint64_t type;
};

template <>
struct floating_point_mapping<long double>{
	typedef std::uintmax_t type;
};

class SerializerStream{
	typedef unsigned objectid_t;
	static const objectid_t null_oid = 0;
	objectid_t next_object_id;
	std::unordered_map<uintptr_t, objectid_t> id_map;
	std::map<objectid_t, ObjectNode> node_map;
	std::ostream *stream;

	objectid_t get_new_oid();
	objectid_t save_object(const void *p);
public:
	SerializerStream(std::ostream &);
	void begin_serialization(const Serializable &obj);
	void serialize_id(const void *p){
		auto it = this->id_map.find((uintptr_t)p);
		if (it == this->id_map.end())
			abort();
		this->serialize(it->second);
	}
	template <typename T>
	void serialize(const T *t){
		this->serialize_id(t);
	}
	template <typename T>
	void serialize(const std::shared_ptr<T> &t){
		this->serialize_id(t.get());
	}
	template <typename T, size_t N>
	void serialize(const (&array)[N]){
		for (const auto &e : array)
			this->serialize(e);
	}
	void serialize(std::uint8_t c){
		this->stream->write((const char *)&c, 1);
	}
	void serialize(std::int8_t c){
		this->stream->write((const char *)&c, 1);
	}
	template <typename T>
	typename std::enable_if<std::is_unsigned<T>::value, void>::type serialize_fixed(T n){
		BOOST_STATIC_ASSERT(CHAR_BITS == 8);

		std::uint8_t array[sizeof(n)];
		for (auto &i : array){
			i = n & 0xFF;
			n >>= 8;
		}
		this->stream->write((const char *)array, sizeof(array));
	}
	template <typename T>
	typename std::enable_if<std::is_signed<T>::value, void>::type serialize(T z){
		this->serialize(ints_to_uints(z));
	}
	template <typename T>
	typename std::enable_if<std::is_unsigned<T>::value, void>::type serialize(T n){
		BOOST_STATIC_ASSERT(CHAR_BITS == 8);

		const unsigned shift = sizeof(n) - 7;

		const std::uint8_t mask = 0x7F;
		while (n > mask){
			std::uint8_t m = n >> shift;
			m &= mask;
			m |= ~mask;
			this->serialize(m);
		}
		this->serialize(n);
	}
	template <typename T>
	void serialize(std::enable_if<std::is_floating_point<T>::value>::type x){
		typedef typename floating_point_mapping<T>::type u;
		u mantissa;
		int exponent;
		to_ints(mantissa, exponent, x);
		this->serialize_fixed(mantissa);
		this->serialize(exponent);
	}
	template <typename T>
	void serialize(const std::basic_string<T> &s){
		this->serialize(s.size());
		for (auto c : s)
			this->serialize(c);
	}
	template <typename It>
	void serialize_sequence(It begin, It end, size_t length){
		this->serialize(length);
		for (; begin != end; ++begin)
			this->serialize(*begin);
	}
	template <typename It>
	void serialize_maplike(It begin, It end, size_t length){
		this->serialize(length);
		for (; begin != end; ++begin){
			this->serialize(begin->first);
			this->serialize(begin->second);
		}
	}
	template <typename T>
	void serialize(const std::vector<T> &v){
		this->serialize_sequence(v.begin(), v.end(), v.size());
	}
	template <typename T>
	void serialize(const std::set<T> &s){
		this->serialize_sequence(s.begin(), s.end(), s.size());
	}
	template <typename T>
	void serialize(const std::unordered_set<T> &s){
		this->serialize_sequence(s.begin(), s.end(), s.size());
	}
	template <typename T>
	void serialize(const std::map<T> &s){
		this->serialize_maplike(s.begin(), s.end(), s.size());
	}
	template <typename T>
	void serialize(const std::unordered_map<T> &s){
		this->serialize_maplike(s.begin(), s.end(), s.size());
	}
};

#endif
