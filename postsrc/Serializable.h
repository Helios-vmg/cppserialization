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
	virtual ObjectNode get_object_node() = 0;
	virtual void serialize(SerializerStream &) const = 0;
	virtual TypeId get_typeid() const = 0;
	Serializable *allocate_memory(TypeId);
	void construct_memory(TypeId, Serializable *, DeserializerStream &);
};

#endif
