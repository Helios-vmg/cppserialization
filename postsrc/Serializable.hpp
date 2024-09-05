#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include "serialization_utils.hpp"
#include "SerializerStream.hpp"
#include <utility>
#include <array>
#include <atomic>
#include <cstdint>

static const std::uint32_t Serializable_type_id = std::numeric_limits<std::uint32_t>::max();

class SerializerStream;
class DeserializerStream;
enum class PointerType;

struct TypeHash{
	unsigned char digest[32];
	TypeHash(){
		memset(digest, 0, sizeof(digest));
	}
	TypeHash(const unsigned char (&digest)[32]){
		memcpy(this->digest, digest, 32);
	}
	bool operator==(const TypeHash &b) const{
		return !memcmp(this->digest, b.digest, 32);
	}
	bool operator!=(const TypeHash &b) const{
		return !(*this == b);
	}
	bool operator<(const TypeHash &b) const{
		return memcmp(this->digest, b.digest, 32) < 0;
	}
};

class SerializableMetadata;

class Serializable{
public:
	typedef std::uintmax_t oid_t;
private:
	static std::atomic<oid_t> next_id;
	oid_t id;
public:
	Serializable(): id(next_id++){}
	virtual ~Serializable(){}
	virtual void get_object_node(std::vector<ObjectNode> &) const = 0;
	ObjectNode get_object_node() const{
		return ObjectNode(
			this,
			[](const void *This, std::vector<ObjectNode> &dst){
				static_cast<const Serializable *>(This)->get_object_node(dst);
			},
			[](const void *This, SerializerStream &ss){
				static_cast<const Serializable *>(This)->serialize(ss);
			},
			this->get_type_id()
		);
	}
	virtual void serialize(SerializerStream &) const = 0;
	virtual std::uint32_t get_type_id() const = 0;
	virtual TypeHash get_type_hash() const = 0;
	virtual std::shared_ptr<SerializableMetadata> get_metadata() const = 0;
	virtual void rollback_deserialization(){}
	oid_t get_id() const{
		return this->id;
	}
};

inline ObjectNode get_object_node(const Serializable *serializable){
	return !serializable ? ObjectNode() : serializable->get_object_node();
}

class DeserializerStream;

class GenericPointer{
public:
	void *pointer;
	virtual ~GenericPointer(){}
	virtual void release() = 0;
	virtual std::unique_ptr<GenericPointer> cast(std::uint32_t type) = 0;
};

enum class CastCategory : char{
	Trivial = 0,
	Complex = 1,
	Invalid = 2,
};

class SerializableMetadata{
public:
	typedef void *(*allocator_t)(std::uint32_t);
	typedef void (*constructor_t)(std::uint32_t, void *, DeserializerStream &);
	typedef void (*rollbacker_t)(std::uint32_t, void *);
	typedef bool (*is_serializable_t)(std::uint32_t);
	//typedef std::vector<std::tuple<std::uint32_t, std::uint32_t, int>> (*cast_offsets_t)();
	typedef Serializable *(*dynamic_cast_f)(void *, std::uint32_t);
	typedef std::unique_ptr<GenericPointer> (*allocate_pointer_t)(std::uint32_t, PointerType, void *);
	typedef CastCategory (*categorize_cast_t)(std::uint32_t, std::uint32_t);
	typedef bool (*check_enum_value_t)(std::uint32_t, const void *);
private:
	std::vector<std::pair<std::uint32_t, TypeHash>> known_types;
	//Used for deserialization.
	std::unique_ptr<std::map<std::uint32_t, std::uint32_t>> typemap;
	allocator_t allocator;
	constructor_t constructor;
	rollbacker_t rollbacker;
	is_serializable_t is_serializable;
	allocate_pointer_t pointer_allocator;
	dynamic_cast_f dynamic_cast_p;
	categorize_cast_t categorizer;
	check_enum_value_t enum_checker;

	std::uint32_t map_type(std::uint32_t);
	std::uint32_t known_type_from_hash(const TypeHash &);
public:
	void add_type(std::uint32_t, const TypeHash &);
	void set_type_mappings(const std::vector<std::pair<std::uint32_t, TypeHash> > &);
	const std::vector<std::pair<std::uint32_t, TypeHash> > &get_known_types() const{
		return this->known_types;
	}
	void set_functions(
			allocator_t allocator,
			constructor_t constructor,
			rollbacker_t rollbacker,
			is_serializable_t is_serializable,
			dynamic_cast_f dynamic_cast_p,
			allocate_pointer_t pointer_allocator,
			categorize_cast_t categorizer,
			check_enum_value_t enum_checker){
		this->allocator = allocator;
		this->constructor = constructor;
		this->rollbacker = rollbacker;
		this->is_serializable = is_serializable;
		this->dynamic_cast_p = dynamic_cast_p;
		this->pointer_allocator = pointer_allocator;
		this->categorizer = categorizer;
		this->enum_checker = enum_checker;
	}
	void *allocate_memory(DeserializerStream &ds, std::uint32_t);
	void construct_memory(std::uint32_t, void *, DeserializerStream &);
	void rollback_construction(std::uint32_t, void *);
	bool type_is_serializable(std::uint32_t);
	//std::vector<std::tuple<std::uint32_t, std::uint32_t, int>> get_cast_offsets();
	Serializable *perform_dynamic_cast(void *p, std::uint32_t type);
	std::unique_ptr<GenericPointer> allocate_pointer(std::uint32_t type, PointerType pointer_type, void *pointer);
	CastCategory categorize_cast(std::uint32_t object_type, std::uint32_t dst_type);
	bool check_enum_value(std::uint32_t type, const void *);
};

#endif
