#include "DeserializerStream.h"
#include "Serializable.h"
#include <cstdint>
#include <cassert>

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

#if defined _DEBUG || defined TESTING_BUILD
#define LOG
#endif

template<class It, class F>
It find_first_true(It begin, It end, const F &f){
	if (begin >= end)
		return end;
	if (f(*begin))
		return begin;
	auto diff = end - begin;
	while (diff > 1){
		auto pivot = begin + diff / 2;
		if (!f(*pivot))
			begin = pivot;
		else
			end = pivot;
		diff = end - begin;
	}
	return end;
}

Serializable *DeserializerStream::perform_deserialization(SerializableMetadata &metadata, bool includes_typehashes){
	this->metadata = &metadata;
	auto &object_types = this->object_types;
	object_types.clear();
	std::vector<std::pair<std::uint32_t, void *> > initialized;
	void *main_object = nullptr;
	try{
		this->state = State::Safe;
		this->state = State::ReadingTypeHashes;
		if (includes_typehashes){
#ifdef LOG
			std::clog << "Reading type hashes...\n";
#endif
			metadata.set_type_mappings(this->read_typehashes());
		}
		this->state = State::Safe;

#ifdef LOG
		std::clog << "Reading node map...\n";
#endif
		std::vector<std::pair<std::uint32_t, objectid_t>> type_map;
		wire_size_t size;
		this->deserialize(size);
		type_map.reserve((size_t)size);
		while (size--){
			std::uint32_t type_id;
			objectid_t object_id;
			this->deserialize(type_id);
			this->deserialize(object_id);
			type_map.push_back(std::make_pair(type_id, object_id));
		}

		objectid_t root_object_id;
		this->deserialize(root_object_id);

#ifdef LOG
		std::clog << "Allocating memory...\n";
#endif
		this->state = State::AllocatingMemory;
		std::uint32_t main_object_type = 0;
		{
			objectid_t object_id = 1;
			for (auto &p : type_map){
				std::uint32_t type_id = p.first;
				for (; object_id <= p.second; object_id++){
					object_types[object_id] = type_id;
					auto mem = metadata.allocate_memory(*this, type_id);
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

#ifdef LOG
		std::clog << "Constructing memory (by deserializing object data)...\n";
#endif
		this->state = State::InitializingObjects;
		for (auto &kv : this->node_map){
			auto type = object_types[kv.first];
			metadata.construct_memory(type, kv.second, *this);
			initialized.push_back(std::make_pair(type, kv.second));
		}
#ifdef LOG
		std::clog << "Checking sanity...\n";
#endif
		this->state = State::SanityCheck;
		//Check that the main object has been set.
		if (!main_object_type || !main_object)
			this->report_error(ErrorType::InvalidProgramState);
		//Check that the main object is an instance of Serializable.
		if (!metadata.type_is_serializable(main_object_type))
			this->report_error(ErrorType::MainObjectNotSerializable);
		//Check smart pointer consistency. In particular, it's an error for an
		//std::unique_ptr and an std::shared_ptr to point to the same object.
		try{
			std::map<objectid_t, PointerType> pointer_types;
			for (auto &p : this->pointers){
				auto it = pointer_types.find(p.object_id);
				if (it == pointer_types.end()){
					pointer_types[p.object_id] = p.pointer_type;
					continue;
				}
				switch (it->second){
					case PointerType::RawPointer:
						it->second = p.pointer_type;
						break;
					case PointerType::UniquePtr:
						if (p.pointer_type == PointerType::SharedPtr)
							this->report_error(ErrorType::InconsistentSmartPointers);
						break;
					case PointerType::SharedPtr:
						if (p.pointer_type == PointerType::UniquePtr)
							this->report_error(ErrorType::InconsistentSmartPointers);
						break;
					default:
						throw InternalErrorException();
				}
			}
		}catch (std::bad_alloc &){
#ifdef LOG
			std::clog << "Allocation error while checking sanity. Continuing anyway...\n";
#endif
		}
		main_object = metadata.perform_dynamic_cast(main_object, main_object_type);
		this->state = State::SettingPointers;
#ifdef LOG
		std::clog << "Setting pointers...\n";
#endif
		{
			try{
				for (auto &p : this->pointers){
					K key(p.object_id, p.pointer_type);
					if (this->base_pointers.find(key) != this->base_pointers.end())
						continue;
					this->base_pointers[key] = metadata.allocate_pointer(p.object_type, p.pointer_type, this->node_map[p.object_id]);
				}
				for (auto &p : this->pointers){
					K key(p.object_id, p.pointer_type);
					auto temp = this->base_pointers[key]->cast(p.pointed_type);
					p.setter(temp->pointer);
				}
				this->base_pointers.clear();
			}catch (std::bad_alloc &){
				for (auto &kv : this->base_pointers)
					kv.second->release();
				this->report_error(ErrorType::OutOfMemory);
			}
		}
		this->state = State::Done;
	}catch (std::bad_alloc &){
		throw;
	}catch (std::exception &){
		switch (this->state){
			case State::Safe:
				break;
			case State::ReadingTypeHashes:
				break;
			case State::SettingPointers:
			case State::SanityCheck:
			case State::InitializingObjects:
				for (auto &p : initialized)
					metadata.rollback_construction(object_types[p.first], p.second);
				for (auto &[k, p] : this->base_pointers)
					p->release();
				this->base_pointers.clear();
			case State::AllocatingMemory:
				for (auto &pair : this->node_map)
					::operator delete(pair.second);
				this->node_map.clear();
				break;
			case State::Done:
				assert(false);
		}
		throw;
	}
#ifdef LOG
		std::clog << "Deserialization done!\n";
#endif
	return (Serializable *)main_object;
}

int DeserializerStream::categorize_cast(std::uint32_t object_type, std::uint32_t dst_type){
	return (int)this->metadata->categorize_cast(object_type, dst_type);
}

std::unique_ptr<GenericPointer> DeserializerStream::allocate_pointer(std::uint32_t object_type, PointerType pointer_type, objectid_t object){
	return this->metadata->allocate_pointer(object_type, pointer_type, this->node_map[object]);
}

void DeserializerStream::set_pointer(void *dst, PointerBackpatch::Setter::callback_t callback, GenericPointer &gp, std::uint32_t pointed_type){
	auto temp = gp.cast(pointed_type);
	callback(dst, temp->pointer);
}
