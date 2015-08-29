#include "DeserializerStream.h"
#include "Serializable.h"

DeserializerStream::DeserializerStream(std::istream &stream): stream(&stream){
}

Serializable *DeserializerStream::begin_deserialization(bool includes_typehashes){
}
