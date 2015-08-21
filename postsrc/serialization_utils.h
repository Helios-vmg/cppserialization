#ifndef SERIALIZATION_UTILS_H
#define SERIALIZATION_UTILS_H

#include <memory>
#include <vector>
#include <utility>
#include <functional>
#include <type_traits>
#include <limits>
#include <iostream>

class SerializerStream;

template <typename T>
std::shared_ptr<T> make_shared(T *p){
	return std::shared_ptr<T>(p);
}

class NodeIterator{
	std::shared_ptr<std::vector<ObjectNode>> nodes;
	size_t state;
public:
	NodeIterator(){
		this->reset();
	}
	NodeIterator(const decltype(NodeIterator::nodes) &nodes): nodes(nodes){
		this->reset();
	}
	bool next(){
		if (!this->nodes)
			return false;
		return ++this->state < this->nodes->size();
	}
	void reset(){
		this->state = std::numeric_limits<decltype(this->state)>::max();
	}
	ObjectNode operator*(){
		return (*this->nodes)[this->state];
	}
	ObjectNode *operator->(){
		return &(*this->nodes)[this->state];
	}
};

struct ObjectNode{
	const void *address;
	std::function<NodeIterator()> get_children;
	typedef std::function<void (SerializerStream &)> serialize_t;
	serialize_t serialize;
	std::function<std::uint32_t()> get_typeid;
};

template <typename T>
ObjectNode get_object_node_default(T *n){
	return {
		n,
		[](){
			return NodeIterator();
		},
		[n](SerializerStream &ss){
			ss.serialize_id(n);
		},
		[](){
			return 0;
		}
	};
}

template <typename T>
ObjectNode get_object_node(bool *n){
	return get_object_node_default(n);
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<bool> &n){
	return get_object_node_default(n.get());
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value, ObjectNode>::type get_object_node(T *n){
	return get_object_node_default(n);
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, ObjectNode>::type get_object_node(T *n){
	return get_object_node_default(n);
}

template <typename T>
ObjectNode get_object_node(std::basic_string<T> *n){
	return get_object_node_default(n);
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<std::basic_string<T> > &n){
	return get_object_node_default(n.get());
}

template <typename T>
ObjectNode get_object_node(T **p){
	return {
		p,
		[p](){
			auto v = make_shared(new std::vector<ObjectNode>);
			v->push_back(get_object_node(*p));
			return NodeIterator(v);
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[](){
			return 0;
		}
	};
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<T *> &n){
	return get_object_node(n.get());
}

template <typename T>
ObjectNode get_object_node(std::shared_ptr<T> *p){
	return {
		p,
		[](){
			return NodeIterator();
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[](){
			return 0;
		}
	};
}

template <typename T>
ObjectNode get_object_node_sequence(T *p){
	return {
		p,
		[](){
			return NodeIterator();
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[](){
			return 0;
		}
	};
}

template <typename T>
ObjectNode get_object_node(std::vector<T> *p){
	return get_object_node_sequence(p);
}

template <typename T>
ObjectNode get_object_node(std::list<T> *p){
	return get_object_node_sequence(p);
}

template <typename T>
ObjectNode get_object_node(std::set<T> *p){
	return get_object_node_sequence(p);
}

template <typename T>
ObjectNode get_object_node(std::unordered_set<T> *p){
	return get_object_node_sequence(p);
}

template <typename T>
ObjectNode get_object_node_assoc(T *p){
	return {
		p,
		[](){
			return NodeIterator();
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[](){
			return 0;
		}
	};
}

template <typename T>
ObjectNode get_object_node(std::map<T> *p){
	return get_object_node_assoc(p);
}

template <typename T>
ObjectNode get_object_node(std::unordered_map<T> *p){
	return get_object_node_assoc(p);
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<T> &n){
	return get_object_node(n.get());
}

inline ObjectNode get_object_node(const Serializable *serializable){
	return serializable->get_object_node();
}

#endif
