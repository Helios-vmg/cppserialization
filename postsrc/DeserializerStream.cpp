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
	std::vector<void *> initialized;
	try{
		this->state = State::ReadingTypeHashes;
		if (includes_typehashes)
			this->set_type_mappings(this->read_typehashes());
		this->state = State::Safe;
		std::uint32_t size;
		this->deserialize(size);
		this->state = State::AllocatingMemory;
		std::map<objectid_t, std::uint32_t> object_types;
		for (auto i = size; i--;){
			objectid_t object_id;
			std::uint32_t type_id;
			this->deserialize(object_id);
			this->deserialize(type_id);
			object_types[object_id] = type_id;
			auto mem = metadata.allocate_memory(type_id);
			try{
				this->node_map[object_id] = mem;
			}catch (...){
				::operator delete(mem);
				throw;
			}
		}
		this->state = State::InitializingObjects;
		for (auto &kv : this->node_map){
			metadata.construct_memory(object_types[kv.first], kv.second, *this);
		}

	}catch (...){
		switch (this->state){
			case State::Safe:
				break;
			case State::ReadingTypeHashes:
				break;
			case State::AllocatingMemory:
				for (auto &pair : this->node_map)
					::operator delete(pair.second);
				this->node_map.clear();
				break;
		}
	}
}
