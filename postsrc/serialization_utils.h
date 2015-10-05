#ifndef SERIALIZATION_UTILS_H
#define SERIALIZATION_UTILS_H

#include <memory>
#include <vector>
#include <utility>
#include <functional>
#include <type_traits>
#include <limits>
#include <iostream>
#include <cstdint>
#include <list>
#include <set>
#include <unordered_set>
#include <map>
#include <unordered_map>

class SerializerStream;

template <typename T>
std::shared_ptr<T> make_shared(T *p){
	return std::shared_ptr<T>(p);
}

class NodeIterator;

struct ObjectNode{
	const void *address;
	std::function<NodeIterator()> get_children;
	typedef std::function<void (SerializerStream &)> serialize_t;
	serialize_t serialize;
	std::function<std::uint32_t()> get_typeid;
};

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

template <typename T>
ObjectNode get_object_node_default(T *n, std::uint32_t type_id){
	return {
		n,
		[](){
			return NodeIterator();
		},
		[n](SerializerStream &ss){
			ss.serialize(*n);
		},
		[type_id](){
			return type_id;
		}
	};
}

template <typename T>
ObjectNode get_object_node(T *n, std::uint32_t type_id){
	return get_object_node_default(n, type_id);
}

template <typename T>
typename std::enable_if<std::is_integral<T>::value, ObjectNode>::type get_object_node(T *n, std::uint32_t type_id){
	return get_object_node_default(n, type_id);
}

template <typename T>
typename std::enable_if<std::is_floating_point<T>::value, ObjectNode>::type get_object_node(T *n, std::uint32_t type_id){
	return get_object_node_default(n, type_id);
}

template <typename T>
ObjectNode get_object_node(std::basic_string<T> *n, std::uint32_t type_id){
	return get_object_node_default(n, type_id);
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<std::basic_string<T> > &n, std::uint32_t type_id){
	return get_object_node_default(n.get(), type_id);
}

template <typename T>
ObjectNode get_object_node(T **p, std::uint32_t type_id){
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
		[type_id](){
			return type_id;
		}
	};
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<T *> &n, std::uint32_t type_id){
	return get_object_node(n.get(), type_id);
}

template <typename T>
ObjectNode get_object_node(std::shared_ptr<T> *p, std::uint32_t type_id){
	return {
		p,
		[](){
			return NodeIterator();
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[type_id](){
			return type_id;
		}
	};
}

template <typename T>
ObjectNode get_object_node_sequence(T *p, std::uint32_t type_id){
	return {
		p,
		[](){
			return NodeIterator();
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[type_id](){
			return type_id;
		}
	};
}

template <typename T>
ObjectNode get_object_node(std::vector<T> *p, std::uint32_t type_id){
	return get_object_node_sequence(p, type_id);
}

template <typename T>
ObjectNode get_object_node(std::list<T> *p, std::uint32_t type_id){
	return get_object_node_sequence(p, type_id);
}

template <typename T>
ObjectNode get_object_node(std::set<T> *p, std::uint32_t type_id){
	return get_object_node_sequence(p, type_id);
}

template <typename T>
ObjectNode get_object_node(std::unordered_set<T> *p, std::uint32_t type_id){
	return get_object_node_sequence(p, type_id);
}

template <typename T>
ObjectNode get_object_node_assoc(T *p, std::uint32_t type_id){
	return {
		p,
		[](){
			return NodeIterator();
		},
		[p](SerializerStream &ss){
			ss.serialize_id(p);
		},
		[type_id](){
			return type_id;
		}
	};
}

template <typename T1, typename T2>
ObjectNode get_object_node(std::map<T1, T2> *p, std::uint32_t type_id){
	return get_object_node_assoc(p, type_id);
}

template <typename T1, typename T2>
ObjectNode get_object_node(std::unordered_map<T1, T2> *p, std::uint32_t type_id){
	return get_object_node_assoc(p, type_id);
}

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<T> &n, std::uint32_t type_id){
	return get_object_node(n.get(), type_id);
}

typedef std::uint64_t wire_size_t;

template <typename T>
struct is_basic_type{
	static const bool value = std::is_fundamental<T>::value || std::is_pointer<T>::value;
};

template <typename Container>
struct is_smart_ptr{
	static const bool value = false;
};

template <typename T> struct is_smart_ptr<std::shared_ptr<T>>{ static const bool value = true; };
template <typename T> struct is_smart_ptr<std::unique_ptr<T>>{ static const bool value = true; };

template <typename Container>
struct is_sequence_container{
	static const bool value = false;
};

template <typename T> struct is_sequence_container<std::vector<T>>{ static const bool value = true; };
template <typename T> struct is_sequence_container<std::list<T>>{ static const bool value = true; };
template <typename T> struct is_sequence_container<std::set<T>>{ static const bool value = true; };
template <typename T> struct is_sequence_container<std::unordered_set<T>>{ static const bool value = true; };

template <typename Container>
struct is_setlike{
	static const bool value = false;
};

template <typename T> struct is_setlike<std::set<T>>{ static const bool value = true; };
template <typename T> struct is_setlike<std::unordered_set<T>>{ static const bool value = true; };

template <typename Container>
struct is_maplike{
	static const bool value = false;
};

template <typename T1, typename T2> struct is_maplike<std::map<T1, T2>>{ static const bool value = true; };
template <typename T1, typename T2> struct is_maplike<std::unordered_map<T1, T2>>{ static const bool value = true; };

template <typename T>
struct is_container{
	static const bool value = is_sequence_container<T>::value || is_maplike<T>::value;
};

template <typename T>
struct is_string{
	static const bool value = false;
};

template <typename T>
struct is_string<std::basic_string<T>>{
	static const bool value = true;
};

template <typename T>
struct is_simply_constructible{
	static const bool value =
		is_basic_type<T>::value ||
		is_smart_ptr<T>::value ||
		is_container<T>::value ||
		is_string<T>::value;
};

template <typename T>
struct is_serializable{
	static const bool value = std::is_base_of<Serializable, T>::value;
};

class DeserializerStream;

template <typename T>
struct proxy_constructor{
	DeserializerStream &ds;
	proxy_constructor(DeserializerStream &ds): ds(ds){}
	operator T() const{
		T temp;
		this->ds.deserialize(temp);
		return std::move(temp);
	}
};

#endif
