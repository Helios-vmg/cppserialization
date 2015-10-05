#include "DeserializerStream.h"
#include "Serializable.h"
#include <cstdint>

DeserializerStream::DeserializerStream(std::istream &stream): stream(&stream){
}

std::vector<std::pair<std::uint32_t, TypeHash> > DeserializerStream::read_typehashes(){
	std::vector<std::pair<std::uint32_t, TypeHash> > ret;
	std::uint32_t size;
	this->deserialize(size);
	ret.reserve(size);
	while (ret.size() < size){
		std::pair<std::uint32_t, TypeHash> pair;
		this->deserialize(pair.first);
		this->deserialize_array(pair.second.digest);
		ret.push_back(pair);
	}
	return ret;
}

Serializable *DeserializerStream::begin_deserialization(SerializableMetadata &metadata, bool includes_typehashes){
	std::map<objectid_t, std::uint32_t> object_types;
	std::vector<std::pair<std::uint32_t, void *> > initialized;
	void *main_object = nullptr;
	try{
		this->state = State::ReadingTypeHashes;
		if (includes_typehashes)
			metadata.set_type_mappings(this->read_typehashes());
		this->state = State::Safe;

		std::vector<std::pair<std::uint32_t, objectid_t>> type_map;
		wire_size_t size;
		this->deserialize(size);
		type_map.reserve((size_t)size);
		for (auto i = size; size--;){
			std::uint32_t type_id;
			objectid_t object_id;
			this->deserialize(type_id);
			this->deserialize(object_id);
			type_map.push_back(std::make_pair(type_id, object_id));
		}

		objectid_t root_object_id;
		this->deserialize(root_object_id);

		this->state = State::AllocatingMemory;
		std::uint32_t main_object_type = 0;
		{
			objectid_t object_id = 1;
			for (auto &p : type_map){
				std::uint32_t type_id = p.first;
				for (; object_id <= p.second; object_id++){
					object_types[object_id] = type_id;
					auto mem = metadata.allocate_memory(type_id);
					if (!mem)
						this->report_error(ErrorType::AllocateAbstractObject);

					if (!main_object && object_id == root_object_id){
						main_object = mem;
						main_object_type = type_id;
					}
					try{
						this->node_map[object_id] = mem;
					}catch (...){
						::operator delete(mem);
						throw;
					}
				}
			}
		}

		this->state = State::InitializingObjects;
		for (auto &kv : this->node_map){
			auto type = object_types[kv.first];
			metadata.construct_memory(type, kv.second, *this);
			initialized.push_back(std::make_pair(type, kv.second));
		}
		this->state = State::SanityCheck;
		if (!main_object_type || !main_object)
			this->report_error(ErrorType::InvalidProgramState);
		if (!metadata.type_is_serializable(main_object_type))
			this->report_error(ErrorType::MainObjectNotSerializable);
		this->state = State::Done;
	}catch (std::bad_alloc &){
		throw;
	}catch (...){
		switch (this->state){
			case State::Safe:
				break;
			case State::ReadingTypeHashes:
				break;
			case State::SanityCheck:
			case State::InitializingObjects:
				for (auto &p : initialized)
					metadata.rollback_construction(object_types[p.first], p.second);
			case State::AllocatingMemory:
				for (auto &pair : this->node_map)
					::operator delete(pair.second);
				this->node_map.clear();
				break;
		}
		throw;
	}
	for (auto &p : this->known_shared_ptrs)
		p.second.second(p.second.first);
	this->known_shared_ptrs.clear();
	for (auto &p : this->known_unique_ptrs)
		p.second.second(p.second.first);
	this->known_unique_ptrs.clear();
	return (Serializable *)main_object;
}
