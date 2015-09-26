#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include "serialization_utils.h"
#include "SerializerStream.h"
#include <utility>
#include <array>

class SerializerStream;
class DeserializerStream;

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
};

class SerializableMetadata;

class Serializable{
public:
	virtual void get_object_node(std::vector<ObjectNode> &) const = 0;
	ObjectNode get_object_node() const{
		return {
			this,
			[this](){
				auto v = make_shared(new std::vector<ObjectNode>);
				this->get_object_node(*v);
				if (!v->size())
					return NodeIterator();
				return NodeIterator(v);
			},
			[this](SerializerStream &ss){
				this->serialize(ss);
			},
			[this](){
				return this->get_type_id();
			}
		};
	}
	virtual void serialize(SerializerStream &) const = 0;
	virtual std::uint32_t get_type_id() const = 0;
	virtual TypeHash get_type_hash() const = 0;
	virtual std::shared_ptr<SerializableMetadata> get_metadata() const = 0;
	virtual void rollback_deserialization() = 0;
};

inline ObjectNode get_object_node(const Serializable *serializable){
	return serializable->get_object_node();
}

class SerializableMetadata{
public:
	typedef void *(*allocator_t)(std::uint32_t);
	typedef void (*constructor_t)(std::uint32_t, void *, DeserializerStream &);
	typedef void (*rollbacker_t)(std::uint32_t, void *);
	typedef bool (*is_serializable_t)(std::uint32_t);
private:
	std::vector<std::pair<std::uint32_t, TypeHash> > known_types;
	//Used for deserialization.
	std::map<std::uint32_t, std::uint32_t> typemap;
	allocator_t allocator;
	constructor_t constructor;
	rollbacker_t rollbacker;
	is_serializable_t is_serializable;
	std::uint32_t map_type(std::uint32_t);
	std::uint32_t known_type_from_hash(const TypeHash &);
public:
	void add_type(std::uint32_t, const TypeHash &);
	void set_type_mappings(const std::vector<std::pair<std::uint32_t, TypeHash> > &);
	const std::vector<std::pair<std::uint32_t, TypeHash> > &get_known_types() const{
		return this->known_types;
	}
	void set_functions(
			const allocator_t &allocator,
			const constructor_t &constructor,
			const rollbacker_t &rollbacker,
			const is_serializable_t &is_serializable){
		this->allocator = allocator;
		this->constructor = constructor;
		this->rollbacker = rollbacker;
		this->is_serializable = is_serializable;
	}
	void *allocate_memory(std::uint32_t);
	void construct_memory(std::uint32_t, void *, DeserializerStream &);
	void rollback_construction(std::uint32_t, void *);
	bool type_is_serializable(std::uint32_t);
};

#endif
