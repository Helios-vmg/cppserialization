#include "SerializerStream.h"
#include "Serializable.h"

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

void SerializerStream::begin_serialization(const Serializable &obj, bool include_typehashes){
	auto node = obj.get_object_node();
#ifdef _DEBUG
	std::clog << "Traversing reference graph...\n";
#endif
	std::vector<decltype(node)> stack;
	stack.push_back(node);
	while (stack.size()){
		auto top = stack.back();
		stack.pop_back();
		auto id = this->save_object(top.address);
		if (!id)
			continue;
		auto it = top.get_children();
		while (it.next())
			stack.push_back(*it);
		this->node_map[id] = top;
	}
	if (include_typehashes){
#ifdef _DEBUG
		std::clog << "Serializing type hashes...\n";
#endif
		auto list = obj.get_metadata()->get_known_types();
		this->serialize((std::uint32_t)list.size());
		for (auto &i : list){
			this->serialize(i.first);
			this->serialize_array(i.second.digest);
		}
	}
#ifdef _DEBUG
	std::clog << "Serializing node map...\n";
#endif
	this->serialize((std::uint32_t)this->node_map.size());
	for (auto &n : this->node_map){
		this->serialize(n.first);
		this->serialize(n.second.get_typeid());
	}
#ifdef _DEBUG
	std::clog << "Serializing nodes...\n";
#endif
	for (auto &n : this->node_map)
		n.second.serialize(*this);
#ifdef _DEBUG
	std::clog << "Serialization done!\n";
#endif
}
