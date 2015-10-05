#include "SerializerStream.h"
#include "Serializable.h"
#include <algorithm>

SerializerStream::SerializerStream(std::ostream &stream):
		stream(&stream),
		next_object_id(1){
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

#if defined _DEBUG
#define LOG
#endif

void SerializerStream::begin_serialization(const Serializable &obj, bool include_typehashes){
	auto node = obj.get_object_node();
#ifdef LOG
	std::clog << "Traversing reference graph...\n";
#endif
	std::vector<decltype(node)> stack;
	stack.push_back(node);
	objectid_t root_object = 0;
	this->id_map[0] = 0;
	while (stack.size()){
		auto top = stack.back();
		stack.pop_back();
		if (!top.address)
			continue;
		auto id = this->save_object(top.address);
		if (!id)
			continue;
		if (!root_object)
			root_object = id;
		auto it = top.get_children();
		while (it.next())
			stack.push_back(*it);
		this->node_map[id] = top;
	}
	if (include_typehashes){
#ifdef LOG
		std::clog << "Serializing type hashes...\n";
#endif
		auto list = obj.get_metadata()->get_known_types();
		this->serialize((std::uint32_t)list.size());
		for (auto &i : list){
			this->serialize(i.first);
			this->serialize_array(i.second.digest);
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
				if (!node.address)
					continue;
				auto oid = this->save_object(node.address);
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
				throw std::exception("Unknown internal serializer error.");
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
