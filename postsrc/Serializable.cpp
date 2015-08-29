#include "Serializable.h"

void *SerializableMetadata::allocate_memory(std::uint32_t type){
	return this->allocator(type);
}

void SerializableMetadata::construct_memory(std::uint32_t type, void *s, DeserializerStream &ds){
	this->constructor(type, s, ds);
}
