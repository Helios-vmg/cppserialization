#ifndef DESERIALIZERSTREAM_H
#define DESERIALIZERSTREAM_H

#include <iostream>
#include <type_traits>
#include <vector>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>
#include <cstdint>
#include <array>
#include "serialization_utils.h"

class Serializable;
struct TypeHash;
class SerializableMetadata;

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
	void deserialize_smart_ptr(T &t, std::map<uintptr_t, smart_ptr_freer_pair_t> &correct, std::map<uintptr_t, smart_ptr_freer_pair_t> &incorrect){
		typename T::element_type *p;
		this->deserialize(p);
		auto it = incorrect.find((uintptr_t)p);
		if (it != incorrect.end())
			throw std::exception("Inconsistent smart pointer usage detected.");
		it = correct.find((uintptr_t)p);
		if (it != correct.end()){
			t = *(T *)it->second.first;
			return;
		}
		auto *sp = new T(p);
		correct[(uintptr_t)p] = smart_ptr_freer_pair_t(sp, [](void *p){ delete (T *)p; });
		t = *sp;
	}
	template <typename SetT, typename ValueT>
	typename std::enable_if<is_simply_constructible<ValueT>::value, void>::type deserialize_setlike(SetT &s){
		s.clear();
		wire_size_t size;
		this->deserialize(size);
		while (s.size() != (size_t)size){
			ValueT temp;
			this->deserialize(temp);
			s.insert(std::move(temp));
		}
	}
	template <typename SetT, typename ValueT>
	typename std::enable_if<is_serializable<ValueT>::value, void>::type deserialize_setlike(SetT &s){
		s.clear();
		wire_size_t size;
		this->deserialize(size);
		while (s.size() != (size_t)size)
			s.emplace(*this);
	}
	template <typename MapT, typename KeyT, typename ValueT>
	typename std::enable_if<
		is_simply_constructible<KeyT>::value &&
		is_simply_constructible<ValueT>::value,
		void
	>::type deserialize_maplike(MapT &m){
		m.clear();
		wire_size_t size;
		this->deserialize(size);
		while (m.size() != (size_t)size){
			KeyT ktemp;
			ValueT vtemp;
			this->deserialize(ktemp);
			this->deserialize(vtemp);
			m[ktemp] = vtemp;
		}
	}
	template <typename MapT, typename KeyT, typename ValueT>
	typename std::enable_if<
		is_simply_constructible<KeyT>::value &&
		is_serializable<ValueT>::value,
		void
	>::type deserialize_maplike(MapT &m){
		m.clear();
		wire_size_t size;
		this->deserialize(size);
		while (m.size() != (size_t)size){
			KeyT ktemp;
			this->deserialize(ktemp);
			m.emplace(std::pair<KeyT &, decltype(*this) &>(ktemp, *this));
		}
	}
	template <typename MapT, typename KeyT, typename ValueT>
	typename std::enable_if<
		is_serializable<KeyT>::value &&
		is_simply_constructible<ValueT>::value,
		void
	>::type deserialize_maplike(MapT &m){
		m.clear();
		wire_size_t size;
		this->deserialize(size);
		typedef decltype(*this) DS;
		proxy_constructor<DS> proxy(*this);
		while (m.size() != (size_t)size)
			m.emplace(std::pair<DS &, decltype(proxy) &>(*this, proxy));
	}
	template <typename MapT, typename KeyT, typename ValueT>
	typename std::enable_if<
		is_serializable<KeyT>::value &&
		is_serializable<ValueT>::value,
		void
	>::type deserialize_maplike(MapT &m){
		m.clear();
		wire_size_t size;
		this->deserialize(size);
		typedef decltype(*this) DS;
		while (m.size() != (size_t)size)
			m.emplace(std::pair<DS &, DS &>(*this, *this));
	}
public:
	DeserializerStream(std::istream &);
	template <typename Target>
	Target *begin_deserialization(bool includes_typehashes = false){
		auto metadata = Target::static_get_metadata();
		auto p = this->begin_deserialization(*metadata, includes_typehashes);
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
		void *temp;
		this->deserialize_id(temp);
		t = (T *)temp;
	}
	template <typename T>
	void deserialize(std::shared_ptr<T> &t){
		this->deserialize_smart_ptr(t, this->known_shared_ptrs, this->known_unique_ptrs);
	}
	template <typename T>
	void deserialize(std::unique_ptr<T> &t){
		this->deserialize(t, this->known_unique_ptrs, this->known_shared_ptrs);
	}
	template <typename T, size_t N>
	void deserialize_array(T (&array)[N]){
		for (auto &e : array)
			this->deserialize(e);
	}
	template <typename T, size_t N>
	void deserialize(std::array<T, N> &array){
		for (auto &e : array)
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
	typename std::enable_if<std::is_signed<T>::value, void>::type deserialize(T &z){
		typedef typename std::make_unsigned<T>::type u;
		u temp;
		this->deserialize(temp);
		z = uints_to_ints(temp);
	}
	template <typename T>
	typename std::enable_if<std::is_unsigned<T>::value, void>::type deserialize(T &n){
		static_assert(CHAR_BIT == 8, "Only 8-bit byte platforms supported!");

		const unsigned shift = 7;

		n = 0;

		const std::uint8_t more_mask = 0x80;
		const std::uint8_t mask = ~more_mask;
		std::uint8_t byte;
		do {
			this->deserialize(byte);
			n <<= shift;
			n |= byte & mask;
		}while ((byte & more_mask) == more_mask);
	}
	template <typename T>
	typename std::enable_if<std::is_floating_point<T>::value, void>::type deserialize(T &x){
		static_assert(std::numeric_limits<T>::is_iec559, "Only iec559 float/doubles supported!");
		typedef typename floating_point_mapping<T>::type u;
		static_assert(sizeof(u) != sizeof(T), "Hard-coded integer type doesn't match the size of requested float type!");
		this->deserialize_fixed(*(const u *)&x);
	}
	template <typename T>
	void deserialize(std::basic_string<T> &s){
		wire_size_t size;
		this->deserialize(size);
		s.resize((size_t)size, (T)'0');
		for (size_t i = 0; i != (size_t)size; i++){
			typename std::make_unsigned<T>::type c;
			this->deserialize(c);
			s[i] = c;
		}
	}
	template <typename T>
	typename std::enable_if<is_simply_constructible<T>::value, void>::type deserialize(std::vector<T> &v){
		v.clear();
		wire_size_t size;
		this->deserialize(size);
		v.reserve((size_t)size);
		while (v.size() != (size_t)size){
			v.resize(v.size() + 1);
			this->deserialize(v.back());
		}
	}
	template <typename T>
	typename std::enable_if<is_serializable<T>::value, void>::type deserialize(std::vector<T> &v){
		v.clear();
		wire_size_t size;
		this->deserialize(size);
		v.reserve((size_t)size);
		while (v.size() != (size_t)size)
			v.emplace_back(*this);
	}
	template <typename T>
	typename std::enable_if<is_setlike<T>::value, void>::type deserialize(T &s){
		this->deserialize_setlike<T, typename T::value_type>(s);
	}
	template <typename T>
	typename std::enable_if<is_maplike<T>::value, void>::type deserialize(T &s){
		this->deserialize_maplike<T, typename T::key_type, typename T::mapped_type>(s);
	}
};

#endif
