#ifndef DESERIALIZERSTREAM_H
#define DESERIALIZERSTREAM_H

#include <iostream>

class Serializable;

template <typename T>
typename std::make_unsigned<T>::type uints_to_ints(T n){
	typedef typename std::make_signed<T>::type s;
	if (!(z % 2))
		return (s)(z / 2);
	return -(s)((x - 1) / 2) - 1;
}

class DeserializerStream{
	typedef std::uint32_t objectid_t;
	std::istream *stream;
	std::map<objectid_t, void *> node_map;
	std::vector<std::pair<std::uint32_t, TypeHash> > read_typehashes();
	enum class State{
		Safe,
		ReadingTypeHashes,
		AllocatingMemory,
		InitializingObjects,
		SanityCheck,
		Done,
	};
	State state;
	typedef std::pair<void *, void(*)(void *)> smart_ptr_freer_pair_t;
	std::map<uintptr_t, smart_ptr_freer_pair_t> known_shared_ptrs;
	std::map<uintptr_t, smart_ptr_freer_pair_t> known_unique_ptrs;

	Serializable *begin_deserialization(SerializableMetadata &, bool includes_typehashes = false);
	template <typename T>
	void deserialize(T &t, smart_ptr_freer_pair_t &correct, smart_ptr_freer_pair_t &incorrect){
		typename T::element_type *p;
		this->deserialize_id(p);
		auto it = incorrect.find((uintptr_t)p);
		if (it != incorrect.end())
			throw std::exception("Inconsistent smart pointer usage detected.");
		it = correct.find((uintptr_t)p);
		if (it != correct.end()){
			t = *(T *)it->second.first;
			return;
		}
		auto *sp = new T(p);
		this->correct[(uintptr_t)p] = smart_ptr_freer_pair_t(sp, [](void *p){ delete (T *)p; });
		t = *sp;
	}
public:
	DeserializerStream(std::istream &);
	template <typename Target>
	Target *begin_deserialization(bool includes_typehashes = false){
		auto metadata = Target::static_get_metadata();
		auto p = this->begin_serialization(*metadata, includes_typehashes);
		auto ret = dynamic_cast<Target *>(p);
		if (!ret)
			delete p;
		return ret;
	}
	void deserialize_id(void *&p){
		objectid_t oid;
		this->deserialize(oid);
		auto it = this->node_map.find(oid);
		if (it == this->node_map.end())
			throw std::exception("Serialized stream contains a reference to an unknown object.");
		p = it->second;
	}
	template <typename T>
	void deserialize(T *&t){
		this->deserialize_id(t);
	}
	template <typename T>
	void deserialize(std::shared_ptr<T> &t){
		this->deserialize(t, this->known_shared_ptrs, this->known_unique_ptrs);
	}
	template <typename T>
	void deserialize(std::unique_ptr<T> &t){
		this->deserialize(t, this->known_unique_ptrs, this->known_shared_ptrs);
	}
	template <typename T, size_t N>
	void deserialize_array(T (&array)[N]){
		this->deserialize<T, N>(array);
	}
	template <typename T, size_t N>
	void deserialize(T (&array)[N]){
		for (const auto &e : array)
			this->deserialize(e);
	}
	void deserialize(std::uint8_t &c){
		this->stream->read((char *)&c, 1);
	}
	void deserialize(std::int8_t &c){
		this->stream->read((char *)&c, 1);
	}
	void deserialize(bool &b){
		std::uint8_t temp;
		this->deserialize(temp);
		b = temp != 0;
	}
	template <typename T>
	typename std::enable_if<std::is_unsigned<T>::value, void>::type deserialize_fixed(T &n){
		static_assert(CHAR_BIT == 8, "Only 8-bit byte platforms supported!");

		std::uint8_t array[sizeof(n)];
		this->stream->read((char *)array, sizeof(array));
		unsigned shift = 0;
		n = 0;
		for (auto i : array){
			n |= (T)i << shift;
			shift += 8;
		}
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
