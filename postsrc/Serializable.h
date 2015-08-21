#ifndef SERIALIZABLE_H
#define SERIALIZABLE_H

#include "serialization_utils.h"

class SerializerStream;
class DeserializerStream;

class Serializable{
public:
	enum class TypeId{
#include "Serializable_TypeId.generated.h"
	};
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
				ss.serialize_id(this);
			},
			[this](){
				return this->get_type_id();
			}
		};
	}
	virtual void serialize(SerializerStream &) const = 0;
	virtual TypeId get_typeid() const = 0;
	Serializable *allocate_memory(TypeId);
	void construct_memory(TypeId, Serializable *, DeserializerStream &);
	virtual std::uint32_t get_type_id() const = 0;
};

#endif
