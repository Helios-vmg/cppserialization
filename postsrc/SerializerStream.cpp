#include "SerializerStream.h"

void SerializerStream::begin_serialization(const Serializable &obj){
	auto node = obj.get_object_node();
	std::stack<decltype(node)> stack;
	stack.push(node);
	while (stack.size()){
		auto top = stack.top();
		stack.pop();
		auto id = this->save_object(top.address);
		if (!id)
			continue;
		auto it = top.get_children();
		while (it.next())
			stack.push(*it);
		this->node_map[id] = top;
	}
	for (auto &n : this->node_map){
		this->serialize(n.first);
		this->serialize(n.second.get_typeid());
	}
	for (auto &n : this->node_map)
		n.second.serialize(*this);
}
