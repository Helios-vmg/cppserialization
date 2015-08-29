#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include "serialization_utils.h"
#include "SerializerStream.h"
#include <utility>

class SerializerStream;
class DeserializerStream;

struct TypeHash{
	unsigned char digest[32];
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
};

inline ObjectNode get_object_node(const Serializable *serializable){
	return serializable->get_object_node();
}

class SerializableMetadata{
	std::vector<std::pair<std::uint32_t, TypeHash> > known_types;
	std::function<Serializable *(std::uint32_t)> allocator;
	std::function<void(std::uint32_t, Serializable *, DeserializerStream &)> constructor;
public:
	void add_type(std::uint32_t, const TypeHash &);
	void set_functions(const std::function<Serializable *(std::uint32_t)> &allocator, const std::function<void(std::uint32_t, Serializable *, DeserializerStream &)> &constructor){
		this->allocator = allocator;
		this->constructor = constructor;
	}
	void *allocate_memory(std::uint32_t);
	void construct_memory(std::uint32_t, void *, DeserializerStream &);
};

#endif
