#ifndef DESERIALIZERSTREAM_H
#define DESERIALIZERSTREAM_H

#include <iostream>

class Serializable;

class DeserializerStream{
	typedef std::uint32_t objectid_t;
	std::istream *stream;
	std::map<objectid_t, void *> node_map;
	std::vector<std::pair<std::uint32_t, TypeHash> > read_typehashes();
	Serializable *begin_deserialization(SerializableMetadata &, bool includes_typehashes = false);
	enum class State{
		Safe,
		ReadingTypeHashes,
		AllocatingMemory,
		InitializingObjects,
	};
	State state;
public:
	DeserializerStream(std::istream &);
	template <typename Target>
	Target *begin_deserialization(bool includes_typehashes = false){
		auto metadata = Target::static_get_metadata();
		auto ret = this->begin_serialization(*metadata, includes_typehashes);
		return dynamic_cast<Target *>(ret);
	}
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
	void serialize_array(const T (&array)[N]){
		this->serialize<T, N>(array);
	}
	template <typename T, size_t N>
	void serialize(const T (&array)[N]){
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
		BOOST_STATIC_ASSERT(CHAR_BIT == 8);

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
		BOOST_STATIC_ASSERT(CHAR_BIT == 8);

		const unsigned shift = sizeof(n) * 8 - 7;
		const std::uint8_t buffer[(sizeof(n) * 8 + 6) / 7 * 2]; //times 2 for safety
		size_t buffer_size = 0;

		const std::uint8_t mask = 0x7F;
		while (n > mask){
			std::uint8_t m = n >> shift;
			n <<= 7;
			m &= mask;
			m |= ~mask;
			buffer[buffer_size++] = m;
		}
		buffer[buffer_size++] = (std::uint8_t)n;
		this->stream->write((const char *)buffer, buffer_size);
	}
	template <typename T>
	typename std::enable_if<std::is_floating_point<T>::value, void>::type serialize(const T &x){
		BOOST_STATIC_ASSERT(std::numeric_limits<T>::is_iec559);
		typedef typename floating_point_mapping<T>::type u;
		this->serialize_fixed(*(const u *)&x);
	}
	template <typename T>
	void serialize(const std::basic_string<T> &s){
		this->serialize(s.size());
		for (typename std::make_unsigned<T>::type c : s)
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
	template <typename T1, typename T2>
	void serialize(const std::map<T1, T2> &s){
		this->serialize_maplike(s.begin(), s.end(), s.size());
	}
	template <typename T1, typename T2>
	void serialize(const std::unordered_map<T1, T2> &s){
		this->serialize_maplike(s.begin(), s.end(), s.size());
	}
};

#endif
