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
#if __cplusplus >= 201703
#define SERIALIZATION_HAVE_STD_OPTIONAL
#include <optional>
#endif
#include "Serializable.h"
#include "serialization_utils.h"

class Serializable;
struct TypeHash;
class SerializableMetadata;

enum class PointerType{
	RawPointer,
	UniquePtr,
	SharedPtr,
};

class GenericPointer;

template <typename T>
void set_pointer(void *dst, void *p){
	*(T *)dst = std::move(*(T *)p);
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
		UnknownEnumValue,
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
	SerializableMetadata *metadata;
	typedef std::pair<objectid_t, PointerType> K;
	typedef std::unique_ptr<GenericPointer> V;
	std::map<K, V> base_pointers;

	std::unique_ptr<Serializable> perform_deserialization(SerializableMetadata &, bool includes_typehashes = false);
	int categorize_cast(std::uint32_t object_type, std::uint32_t dst_type);
	std::unique_ptr<GenericPointer> allocate_pointer(std::uint32_t object_type, PointerType pointer_type, objectid_t object);
	void set_pointer(void *pointer, PointerBackpatch::Setter::callback_t callback, GenericPointer &gp, std::uint32_t pointed_type);
	template <typename T, typename T2>
	void deserialize_ptr(T &t, PointerType pointer_type){
		objectid_t oid;
		this->deserialize(oid);
		if (!oid){
			t = nullptr;
			return;
		}
		auto object_type = this->object_types[oid];
		auto dst_type = static_get_type_id<T2>::value;
		switch (dst_type == object_type ? 0 : this->categorize_cast(object_type, dst_type)){
			case 0:
				{
					K key(oid, pointer_type);
					GenericPointer *gp = nullptr;
					auto found = this->base_pointers.find(key);
					if (found == this->base_pointers.end()){
						auto temp = this->allocate_pointer(object_type, pointer_type, oid);
						if (pointer_type == PointerType::UniquePtr){
							::set_pointer<T>(&t, temp->pointer);
							this->base_pointers[key] = {};
						}else{
							gp = temp.get();
							this->base_pointers[key] = std::move(temp);
						}
					}else{
						gp = found->second.get();
						if (!gp)
							throw std::runtime_error("Multiple unique pointers to single object detected. (Is this correct?)");
					}
					if (gp)
						this->set_pointer(&t, &::set_pointer<T>, *gp, dst_type);
				}
				return;
			case 1:
				break;
			default:
				this->report_error(ErrorType::InvalidCast);
		}
		PointerBackpatch pb;
		pb.pointed_type = dst_type;
		pb.object_type = object_type;
		pb.object_id = oid;
		pb.pointer_type = pointer_type;
		pb.setter.dst = &t;
		pb.setter.callback = &::set_pointer<T>;
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
public:
	DeserializerStream(std::istream &);
	virtual ~DeserializerStream(){}
	virtual void report_error(ErrorType);
	template <typename Target>
	std::unique_ptr<Target> full_deserialization(bool includes_typehashes = false){
		auto metadata = Target::static_get_metadata();
		auto p = this->perform_deserialization(*metadata, includes_typehashes);
		auto ret = dynamic_cast<Target *>(p.get());
		if (!ret)
			return {};
		p.release();
		return std::unique_ptr<Target>(ret);
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
	void deserialize(std::string &s){
		wire_size_t size;
		this->deserialize(size);
		s.resize((size_t)size, (char)0);
		this->stream->read(s.data(), s.size());
		if (this->stream->gcount() != s.size())
			this->report_error(ErrorType::UnexpectedEndOfFile);
	}
	template <typename T>
	void deserialize(std::basic_string<T> &s){
		wire_size_t size;
		this->deserialize(size);
		s.resize((size_t)size, (T)0);
		for (size_t i = 0; i != (size_t)size; i++){
			typename std::make_unsigned<T>::type c;
			this->deserialize(c);
			s[i] = c;
		}
	}
	template <typename T>
	std::enable_if_t<std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t>, void>
	deserialize(std::vector<T> &s){
		wire_size_t size;
		this->deserialize(size);
		s.resize((size_t)size, (char)0);
		this->stream->read((char *)s.data(), s.size());
		if (this->stream->gcount() != s.size())
			this->report_error(ErrorType::UnexpectedEndOfFile);
	}
	template <typename T>
	std::enable_if_t<is_simply_constructible<T>::value && !(std::is_same_v<T, std::uint8_t> || std::is_same_v<T, std::int8_t>), void>
	deserialize(std::vector<T> &v){
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
	typename std::enable_if<is_serializable<T>::value, void>::type
	deserialize(std::vector<T> &v){
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
#ifdef SERIALIZATION_HAVE_STD_OPTIONAL
	template <typename T>
	void deserialize(std::optional<T> &o){
		bool set;
		this->deserialize(set);
		if (!set){
			o = {};
			return;
		}
		static_assert(std::is_move_constructible<T>::value, "Can't move T");
		T temp;
		this->deserialize(temp);
		o = std::move(temp);
	}
#endif
	template <typename T>
	std::enable_if_t<std::is_enum_v<T>, void>
	deserialize(T &t){
		std::underlying_type_t<T> temp;
		this->deserialize(temp);
		if (!this->metadata->check_enum_value(get_enum_type_id<T>::value, &temp))
			this->report_error(ErrorType::UnknownEnumValue);
		t = (T)temp;
	}
};

class DeserializationException : public std::exception{
protected:
	const char *message;
	DeserializerStream::ErrorType type;
public:
	DeserializationException(DeserializerStream::ErrorType type);
	const char *what() const noexcept override{
		return this->message;
	}
	auto get_type() const{
		return this->type;
	}
};

#endif
