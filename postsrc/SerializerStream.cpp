#include "SerializerStream.h"
#include "Serializable.h"
#include <algorithm>
#include <cassert>

SerializerStream::SerializerStream(std::ostream &stream):
	next_object_id(1),
	stream(&stream){
}

SerializerStream::objectid_t SerializerStream::get_new_oid(){
	return this->next_object_id++;
}

SerializerStream::objectid_t SerializerStream::save_object(const void *p){
	auto up = (uintptr_t)p;
	auto it = this->id_map.find(up);
	if (it != this->id_map.end())
		return null_oid;
	auto ret = this->get_new_oid();
	this->id_map[up] = ret;
	return ret;
}

#if defined _DEBUG || defined TESTING_BUILD
#define LOG
#endif

void SerializerStream::serialize(const Serializable &obj, bool include_typehashes){
	auto node = obj.get_object_node();
#ifdef LOG
	std::clog << "Traversing reference graph...\n";
#endif
	std::set<std::uint32_t> used_types;
	objectid_t root_object = 0;
	{
		std::vector<decltype(node)> stack, temp_stack;
		this->id_map[0] = 0;

		auto id = this->save_object(node.get_address());
		assert(id);
		root_object = id;
		this->node_map[id] = node;
		used_types.insert(node.get_typeid());
		stack.push_back(node);

		while (stack.size()){
			auto top = stack.back();
			stack.pop_back();
			if (!top.get_address())
				continue;
			top.get_children(temp_stack);
			for (auto &i : temp_stack){
				id = this->save_object(i.get_address());
				if (!id)
					continue;
				this->node_map[id] = i;
				used_types.insert(i.get_typeid());
				stack.push_back(i);
			}
			temp_stack.clear();
		}
	}
	if (include_typehashes){
#ifdef LOG
		std::clog <<
			"Travesal found " << this->node_map.size() << " objects.\n"
			"Serializing type hashes...\n";
#endif
		auto list = obj.get_metadata()->get_known_types();
		std::map<decltype(list[0].first), decltype(list[0].second) *> typemap;
		for (auto &i : list)
			typemap[i.first] = &i.second;
		this->serialize((std::uint32_t)used_types.size());
		for (auto &t : used_types){
			this->serialize(t);
			this->serialize_array(typemap[t]->digest);
		}
	}
#ifdef LOG
	std::clog << "Remapping object IDs...\n";
#endif

	{
		std::map<std::uint32_t, std::vector<objectid_t>> remap;
		for (auto &n : this->node_map)
			remap[n.second.get_typeid()].push_back(n.first);
		
		this->next_object_id = 1;
		decltype(this->node_map) temp;
		bool root_set = false;
		this->id_map.clear();
		this->id_map[0] = 0;
		for (auto &r : remap){
			for (auto i : r.second){
				auto &node = this->node_map[i];
				if (!node.get_address())
					continue;
				auto oid = this->save_object(node.get_address());
				if (!root_set && i == root_object){
					root_object = oid;
					root_set = true;
				}
				temp[oid] = node;
			}
		}
		this->node_map = temp;
	}

#ifdef LOG
	std::clog << "Serializing node map...\n";
#endif

	{
		std::vector<std::pair<std::uint32_t, objectid_t>> type_map;
		for (auto &n : this->node_map){
			auto type = n.second.get_typeid();
			if (!type)
				throw InternalErrorException();
			if (!type_map.size() || type != type_map.back().first){
				type_map.push_back(std::make_pair(type, n.first));
				continue;
			}
			type_map.back().second = std::max(type_map.back().second, n.first);
		}
		this->serialize((wire_size_t)type_map.size());
		for (auto &i : type_map){
			this->serialize(i.first);
			this->serialize(i.second);
		}
		this->serialize(root_object);
	}

#ifdef LOG
	std::clog << "Serializing nodes...\n";
#endif
	for (auto &n : this->node_map)
		n.second.serialize(*this);
#ifdef LOG
	std::clog << "Serialization done!\n";
#endif
}
