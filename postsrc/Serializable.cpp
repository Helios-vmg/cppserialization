#include "Serializable.h"

void *SerializableMetadata::allocate_memory(std::uint32_t type){
	return this->allocator(this->map_type(type));
}

std::uint32_t SerializableMetadata::map_type(std::uint32_t input){
	if (!this->typemap.size())
		return input;
	auto it = this->typemap.find(input);
	if (it == this->typemap.end())
		return 0;
	return it->second;
}

void SerializableMetadata::construct_memory(std::uint32_t type, void *s, DeserializerStream &ds){
	this->constructor(type, s, ds);
}

std::uint32_t SerializableMetadata::known_type_from_hash(const TypeHash &hash){
	for (auto &p : this->known_types)
		if (p.second == hash)
			return p.first;
	return 0;
}

void SerializableMetadata::set_type_mappings(const std::vector<std::pair<std::uint32_t, TypeHash> > &typehashes){
	this->typemap.clear();
	for (auto &p : typehashes){
		auto known_type = this->known_type_from_hash(p.second);
		if (!known_type)
			continue;
		this->typemap[p.first] = known_type;
	}
}
