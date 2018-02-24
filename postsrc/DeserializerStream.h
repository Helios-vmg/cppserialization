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

enum class PointerType{
	RawPointer,
	UniquePtr,
	SharedPtr,
};

class DeserializerStream{
public:
	enum class ErrorType{
		UnexpectedEndOfFile,
		InconsistentSmartPointers,
		UnknownObjectId,
		InvalidProgramState,
		MainObjectNotSerializable,
		AllocateAbstractObject,
		AllocateObjectOfUnknownType,
		InvalidCast,
		OutOfMemory,
	};
private:
	typedef std::uint32_t objectid_t;
	
	struct PointerBackpatch{
		std::uint32_t pointed_type;
		std::uint32_t object_type;
		objectid_t object_id;
		PointerType pointer_type;
		struct Setter{
			typedef void (*callback_t)(void *dst, void *p);
			void *dst;
			callback_t callback;
			void operator()(void *p) const{
				this->callback(this->dst, p);
			}
		};
		Setter setter;
	};

	std::istream *stream;
	std::map<objectid_t, void *> node_map;
	std::map<objectid_t, std::uint32_t> object_types;
	std::vector<std::pair<std::uint32_t, TypeHash>> read_typehashes();
	enum class State{
		Safe,
		ReadingTypeHashes,
		AllocatingMemory,
		InitializingObjects,
		SanityCheck,
		SettingPointers,
		Done,
	};
	State state;
	std::vector<PointerBackpatch> pointers;

	Serializable *perform_deserialization(SerializableMetadata &, bool includes_typehashes = false);
	template <typename T, typename T2>
	void deserialize_ptr(T &t, PointerType pointer_type){
		objectid_t oid;
		this->deserialize(oid);
		PointerBackpatch pb;
		pb.pointed_type = static_get_type_id<T2>::value;
		pb.object_type = this->object_types[oid];
		pb.object_id = oid;
		pb.pointer_type = pointer_type;
		pb.setter.dst = &t;
		pb.setter.callback = [](void *dst, void *p){
			*(T *)dst = std::move(*(T *)p);
		};
		this->pointers.push_back(pb);
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
	void *cast_pointer(void *src, std::uint32_t src_type, std::uint32_t dst_type);
public:
	DeserializerStream(std::istream &);
	virtual ~DeserializerStream(){}
	virtual void report_error(ErrorType, const char * = 0) = 0;
	template <typename Target>
	Target *full_deserialization(bool includes_typehashes = false){
		auto metadata = Target::static_get_metadata();
		auto p = this->perform_deserialization(*metadata, includes_typehashes);
		auto ret = dynamic_cast<Target *>(p);
		if (!ret)
			delete p;
		return ret;
	}
	void *deserialize_id(void *&p, std::uint32_t dst_type);
	template <typename T>
	void deserialize_immediately(T *&t){
		void *temp;
		this->deserialize_id(temp, static_get_type_id<T>::value);
		t = (T *)temp;
	}
	template <typename T>
	void deserialize(T *&t){
		this->deserialize_ptr<T *, T>(t, PointerType::RawPointer);
	}
	template <typename T>
	void deserialize(std::shared_ptr<T> &t){
		this->deserialize_ptr<std::shared_ptr<T>, T>(t, PointerType::SharedPtr);
	}
	template <typename T>
	void deserialize(std::unique_ptr<T> &t){
		this->deserialize_ptr<std::unique_ptr<T>, T>(t, PointerType::UniquePtr);
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
		if (this->stream->gcount() < 1)
			this->report_error(ErrorType::UnexpectedEndOfFile);
	}
	void deserialize(std::int8_t &c){
		this->stream->read((char *)&c, 1);
		if (this->stream->gcount() < 1)
			this->report_error(ErrorType::UnexpectedEndOfFile);
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
		if (!*this->stream)
			this->report_error(ErrorType::UnexpectedEndOfFile);
		this->stream->read((char *)array, sizeof(array));
		auto read = this->stream->gcount();
		if (read != sizeof(array))
			this->report_error(ErrorType::UnexpectedEndOfFile);
		unsigned shift = 0;
		n = 0;
		for (auto i : array){
			n |= (T)i << shift;
			shift += 8;
		}
	}
	template <typename T>
	typename std::enable_if<std::is_signed<T>::value && std::is_integral<T>::value, void>::type deserialize(T &z){
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
		static_assert(sizeof(u) == sizeof(T), "Hard-coded integer type doesn't match the size of requested float type!");
		this->deserialize_fixed(*(u *)&x);
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
