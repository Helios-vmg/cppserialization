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
		std::uint32_t size;
		this->deserialize(size);
		this->state = State::AllocatingMemory;
		std::uint32_t main_object_type = 0;
		for (auto i = size; i--;){
			objectid_t object_id;
			std::uint32_t type_id;
			this->deserialize(object_id);
			this->deserialize(type_id);
			object_types[object_id] = type_id;
			auto mem = metadata.allocate_memory(type_id);
			if (!main_object){
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
		this->state = State::InitializingObjects;
		for (auto &kv : this->node_map){
			auto type = object_types[kv.first];
			metadata.construct_memory(type, kv.second, *this);
			initialized.push_back(std::make_pair(type, kv.second));
		}
		this->state = State::SanityCheck;
		if (!main_object_type || !main_object)
			throw std::exception("Program in unknown state!");
		if (!metadata.type_is_serializable(main_object_type))
			throw std::exception("Main object is not an instance of Serializable. Deserilization cannot continue.");
		this->state = State::Done;
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
	}
	for (auto &p : this->known_shared_ptrs)
		p.second.second(p.second.first);
	this->known_shared_ptrs.clear();
	for (auto &p : this->known_unique_ptrs)
		p.second.second(p.second.first);
	this->known_unique_ptrs.clear();
	return (Serializable *)main_object;
}
