#include "Serializable.h"
#include "DeserializerStream.h"

std::atomic<Serializable::oid_t> Serializable::next_id;

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

//std::vector<std::tuple<std::uint32_t, std::uint32_t, int>> SerializableMetadata::get_cast_offsets(){
//	return this->cast_offsets();
//}

Serializable *SerializableMetadata::perform_dynamic_cast(void *p, std::uint32_t type){
	return this->dynamic_cast_p(p, type);
}

std::unique_ptr<GenericPointer> SerializableMetadata::allocate_pointer(std::uint32_t type, PointerType pointer_type, void *pointer){
	return this->pointer_allocator(type, pointer_type, pointer);
}

CastCategory SerializableMetadata::categorize_cast(std::uint32_t object_type, std::uint32_t dst_type){
	return this->categorizer(object_type, dst_type);
}

bool SerializableMetadata::check_enum_value(std::uint32_t type, const void *value){
	return this->enum_checker(type, value);
}

std::pair<bool, uintptr_t> ObjectNode::get_identity() const{
	uintptr_t id;
	if (this->is_serializable)
		id = (uintptr_t)((const Serializable *)this->address)->get_id();
	else
		id = (uintptr_t)this->address;
	return std::make_pair(this->is_serializable, id);
}
