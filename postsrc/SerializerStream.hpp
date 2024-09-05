#pragma once

#include <iostream>
#include <cstdint>
#include <cmath>
#include <type_traits>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <stack>
#include <climits>
#include <array>
#define SERIALIZATION_HAVE_STD_OPTIONAL
#include <optional>
#include "serialization_utils.hpp"
#include "noexcept.hpp"

class Serializable;

class InternalErrorException : public std::exception{
public:
	const char *what() const NOEXCEPT override{
		return "Unknown internal serializer error.";
	}
};

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

template <typename T>
struct is_built_in_type{
	static const bool value = std::is_integral<T>::value || std::is_pointer<T>::value || std::is_floating_point<T>::value || std::is_enum<T>::value;
};

class SerializerStream{
	typedef std::uint32_t objectid_t;
	static const objectid_t null_oid = 0;
	objectid_t next_object_id;
	std::map<std::pair<bool, uintptr_t>, objectid_t> id_map;
	std::map<objectid_t, ObjectNode> node_map;
	std::ostream *stream;

	objectid_t get_new_oid();
	objectid_t save_object(const std::pair<bool, uintptr_t> &p);
	void serialize_id_private(const void *p);
	template <typename T>
	typename std::enable_if<is_built_in_type<T>::value, void>::type
	serialize_id(const T *p){
		this->serialize_id_private(p);
	}
	void serialize_id(const std::string *p){
		this->serialize_id_private(p);
	}
	void serialize_id(const std::wstring *p){
		this->serialize_id_private(p);
	}
	void serialize_id(const Serializable *p);
public:
	SerializerStream(std::ostream &);
	class Options{
	public:
		bool include_typehashes = false;
		//native type ID -> protocol type ID
		const std::unordered_map<std::uint32_t, std::uint32_t> *type_map = nullptr;
		bool remap_object_ids = true;
	};
	//Returns false if the serialization could not be performed.
	bool full_serialization(const Serializable &obj, const Options &);
	void full_serialization(const Serializable &obj){
		this->full_serialization(obj, {});
	}
	void full_serialization(const Serializable &obj, bool include_typehashes){
		Options o{ include_typehashes };
		this->full_serialization(obj, o);
	}
	bool full_serialization(const Serializable &obj, const std::unordered_map<std::uint32_t, std::uint32_t> &type_map){
		Options o{ false, &type_map };
		return this->full_serialization(obj, o);
	}
	template <typename T>
	void serialize(const T *t){
		this->serialize_id(t);
	}
	template <typename T>
	void serialize(const std::unique_ptr<T> &t){
		this->serialize_id(t.get());
	}
	template <typename T>
	void serialize(const std::shared_ptr<T> &t){
		this->serialize_id(t.get());
	}
	template <typename T, size_t N>
	void serialize_array(const T (&array)[N]){
		for (const auto &e : array)
			this->serialize(e);
	}
	template <typename T, size_t N>
	void serialize(const std::array<T, N> &array){
		for (const auto &e : array)
			this->serialize(e);
	}
	void serialize(std::uint8_t c){
		this->stream->write((const char *)&c, 1);
	}
	void serialize(bool b){
		this->serialize((std::uint8_t)b);
	}
	void serialize(std::int8_t c){
		this->stream->write((const char *)&c, 1);
	}
	template <typename T>
	typename std::enable_if<std::is_unsigned<T>::value, void>::type serialize_fixed(T n){
		static_assert(CHAR_BIT == 8, "Only 8-bit byte platforms supported!");

		std::uint8_t array[sizeof(n)];
		for (auto &i : array){
			i = n & 0xFF;
			n >>= 8;
		}
		this->stream->write((const char *)array, sizeof(array));
	}
	template <typename T>
	typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value, void>::type serialize(T z){
		this->serialize(ints_to_uints(z));
	}
	template <typename T>
	typename std::enable_if<std::is_unsigned<T>::value, void>::type serialize(T n){
		static_assert(CHAR_BIT == 8, "Only 8-bit byte platforms supported!");

		if (!n){
			this->serialize((std::uint8_t)0);
			return;
		}

		const unsigned shift = 7;
		const size_t capacity = (sizeof(n) * 8 + 6) / 7 * 2;
		std::uint8_t buffer[capacity]; //times 2 for safety
		const std::uint8_t mask = 0x7F;

		size_t buffer_size = 0;

		while (n > 0){
			std::uint8_t m = n & mask;
			n >>= shift;
			m &= mask;
			m |= ~mask;
			buffer[capacity - 1 - buffer_size++] = m;
		}
		buffer[capacity - 1] &= mask;
		this->stream->write((const char *)buffer + (capacity - buffer_size), buffer_size);
	}
	template <typename T>
	typename std::enable_if<std::is_floating_point<T>::value, void>::type serialize(T x){
		static_assert(std::numeric_limits<T>::is_iec559, "Only iec559 float/doubles supported!");
		typedef typename floating_point_mapping<T>::type U;
		static_assert(sizeof(U) == sizeof(T), "Hard-coded integer type doesn't match the size of requested float type!");
		union{
			T floating;
			U integer;
		} u;
		u.floating = x;
		this->serialize_fixed(u.integer);
	}
	void serialize(const std::string &s){
		this->serialize((wire_size_t)s.size());
		this->stream->write(s.data(), s.size());
	}
	template <typename T>
	void serialize(const std::basic_string<T> &s){
		this->serialize((wire_size_t)s.size());
		for (typename std::make_unsigned<T>::type c : s)
			this->serialize(c);
	}
	template <typename It>
	void serialize_sequence(It begin, It end, size_t length){
		this->serialize((wire_size_t)length);
		for (; begin != end; ++begin)
			this->serialize(*begin);
	}
	template <typename It>
	void serialize_maplike(It begin, It end, size_t length){
		this->serialize((wire_size_t)length);
		for (; begin != end; ++begin){
			this->serialize(begin->first);
			this->serialize(begin->second);
		}
	}
	template <typename T>
	std::enable_if_t<std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t>, void>
	serialize(const std::vector<T> &v){
		this->serialize((wire_size_t)v.size());
		this->stream->write((const char *)v.data(), v.size());
	}
	template <typename T>
	std::enable_if_t<!(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t>), void>
	serialize(const std::vector<T> &v){
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
	template <typename T1, typename T2>
	void serialize(const std::map<T1, T2> &s){
		this->serialize_maplike(s.begin(), s.end(), s.size());
	}
	template <typename T1, typename T2>
	void serialize(const std::unordered_map<T1, T2> &s){
		this->serialize_maplike(s.begin(), s.end(), s.size());
	}
	template <typename T>
	typename std::enable_if<std::is_base_of<Serializable, T>::value, void>::type serialize(const T &serializable){
		serializable.serialize(*this);
	}
	template <typename T>
	std::enable_if_t<std::is_enum_v<T>, void>
	serialize(T t){
		this->serialize((std::underlying_type_t<T>)t);
	}
	template <typename T>
	void serialize(const std::optional<T> &o){
		if (!o){
			this->serialize(false);
			return;
		}
		this->serialize(true);
		this->serialize(*o);
	}
};
