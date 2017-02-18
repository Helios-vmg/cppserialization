#include "Serializable.h"
#include "DeserializerStream.h"

void *SerializableMetadata::allocate_memory(DeserializerStream &ds, std::uint32_t type){
	type = this->map_type(type);
	if (!type)
		ds.report_error(DeserializerStream::ErrorType::AllocateObjectOfUnknownType);
	return this->allocator(type);
}

std::uint32_t SerializableMetadata::map_type(std::uint32_t input){
	if (!this->typemap)
		return input;
	auto it = this->typemap->find(input);
	if (it == this->typemap->end())
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

void SerializableMetadata::add_type(std::uint32_t type, const TypeHash &type_hash){
	this->known_types.push_back(std::make_pair(type, type_hash));
}

void SerializableMetadata::set_type_mappings(const std::vector<std::pair<std::uint32_t, TypeHash> > &typehashes){
	this->typemap.reset(new decltype(this->typemap)::element_type);
	for (auto &p : typehashes){
		auto known_type = this->known_type_from_hash(p.second);
		if (!known_type)
			continue;
		(*this->typemap)[p.first] = known_type;
	}
}

void SerializableMetadata::rollback_construction(std::uint32_t type, void *p){
	this->rollbacker(type, p);
}

bool SerializableMetadata::type_is_serializable(std::uint32_t type){
	return this->is_serializable(type);
}
