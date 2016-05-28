#ifndef SERIALIZATION_UTILS_INL
#define SERIALIZATION_UTILS_INL

#include "SerializerStream.h"
#include "DeserializerStream.h"

template <typename T>
ObjectNode get_object_node_default(T *n, std::uint32_t type_id){
	return ObjectNode(
		n,
		[](const void *This, SerializerStream &ss){
			ss.serialize(*(const T *)This);
		},
		type_id
	);
}

template <typename T>
ObjectNode get_object_node(T **p, std::uint32_t type_id){
	return ObjectNode(
		p,
		[](const void *This, std::vector<ObjectNode> &dst){
			dst.push_back(get_object_node(*(T **)This));
		},
		[](const void *This, SerializerStream &ss){
			ss.serialize_id((T **)This);
		},
		type_id
	);
}

template <typename T>
ObjectNode get_object_node(std::shared_ptr<T> *p, std::uint32_t type_id){
	return ObjectNode(
		p,
		[](const void *This, SerializerStream &ss){
			ss.serialize_id((std::shared_ptr<T> *)This);
		},
		type_id
	);
}

template <typename T>
ObjectNode get_object_node_sequence(T *p, std::uint32_t type_id){
	return ObjectNode(
		p,
		[](const void *This, SerializerStream &ss){
			ss.serialize_id((T *)This);
		},
		type_id
	);
}

template <typename T>
ObjectNode get_object_node_assoc(T *p, std::uint32_t type_id){
	return ObjectNode(
		p,
		[](const void *This, SerializerStream &ss){
			ss.serialize_id((T *)This);
		},
		type_id
	);
}

template <typename T>
proxy_constructor<T>::operator T() const{
	T temp;
	this->ds.deserialize(temp);
	return std::move(temp);
}

#endif
