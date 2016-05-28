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

class ObjectNode{
public:
	typedef void (*get_children_t)(const void *, std::vector<ObjectNode> &);
	typedef void (*serialize_t)(const void *, SerializerStream &);
	typedef std::uint32_t (*get_typeid_t)(const void *);
private:
	const void *address;
	get_children_t get_children_f;
	serialize_t serialize_f;
	std::uint32_t type_id;
public:
	ObjectNode()
		: address(nullptr)
		, get_children_f(nullptr)
		, serialize_f(nullptr)
		, type_id(0){}
	ObjectNode(const void *address, get_children_t get_children_f, serialize_t serialize_f, std::uint32_t type_id)
		: address(address)
		, get_children_f(get_children_f)
		, serialize_f(serialize_f)
		, type_id(type_id){}
	ObjectNode(const void *address, serialize_t serialize_f, std::uint32_t type_id)
		: address(address)
		, get_children_f(nullptr)
		, serialize_f(serialize_f)
		, type_id(type_id){}
	void get_children(std::vector<ObjectNode> &dst){
		if (this->get_children_f)
			this->get_children_f(this->address, dst);
	}
	void serialize(SerializerStream &ss){
		this->serialize_f(this->address, ss);
	}
	std::uint32_t get_typeid(){
		return this->type_id;
	}
	const void *get_address() const{
		return this->address;
	}
};

template <typename T>
ObjectNode get_object_node_default(T *n, std::uint32_t type_id);

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
ObjectNode get_object_node(T **p, std::uint32_t type_id);

template <typename T>
ObjectNode get_object_node(const std::shared_ptr<T *> &n, std::uint32_t type_id){
	return get_object_node(n.get(), type_id);
}

template <typename T>
ObjectNode get_object_node(std::shared_ptr<T> *p, std::uint32_t type_id);

template <typename T>
ObjectNode get_object_node_sequence(T *p, std::uint32_t type_id);

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
ObjectNode get_object_node_assoc(T *p, std::uint32_t type_id);

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

class Serializable;

template <typename T>
struct is_serializable{
	static const bool value = std::is_base_of<Serializable, T>::value;
};

class DeserializerStream;

template <typename T>
struct proxy_constructor{
	DeserializerStream &ds;
	proxy_constructor(DeserializerStream &ds): ds(ds){}
	operator T() const;
};

#endif
