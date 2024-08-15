#include "DeserializerStream.h"

class DeserializationException : public std::exception{
protected:
	const char *message;
	DeserializerStream::ErrorType type;
public:
	DeserializationException(DeserializerStream::ErrorType type): type(type){
		switch (type){
			case DeserializerStream::ErrorType::UnexpectedEndOfFile:
				this->message = "DeserializationError: Unexpected end of file.";
				break;
			case DeserializerStream::ErrorType::InconsistentSmartPointers:
				this->message = "DeserializationError: Serialized stream uses smart pointers inconsistently.";
				break;
			case DeserializerStream::ErrorType::UnknownObjectId:
				this->message = "DeserializationError: Serialized stream contains a reference to an unknown object.";
				break;
			case DeserializerStream::ErrorType::InvalidProgramState:
				this->message = "DeserializationError: The program is in an unknown state.";
				break;
			case DeserializerStream::ErrorType::MainObjectNotSerializable:
				this->message = "DeserializationError: The root object is not an instance of a Serializable subclass.";
				break;
			case DeserializerStream::ErrorType::AllocateAbstractObject:
				this->message = "DeserializationError: The stream contains a concrete object with an abstract class type ID.";
				break;
			case DeserializerStream::ErrorType::AllocateObjectOfUnknownType:
				this->message = "DeserializationError: The stream contains an object of an unknown type.";
				break;
			case DeserializerStream::ErrorType::InvalidCast:
				this->message = "DeserializationError: Deserializing the object would require performing an invalid cast.";
				break;
			case DeserializerStream::ErrorType::OutOfMemory:
				this->message = "DeserializationError: Out of memory.";
				break;
			case DeserializerStream::ErrorType::UnknownEnumValue:
				this->message = "DeserializationError: The enum's underlying type contains a value not understood by the enum.";
				break;
			default:
				this->message = "DeserializationError: Unknown.";
				break;
		}
	}
	const char *what() const noexcept override{
		return this->message;
	}
	auto get_type() const{
		return this->type;
	}
};

class ExampleDeserializerStream : public DeserializerStream{
protected:
	void report_error(ErrorType type, const char *info) override{
		throw DeserializationException(type);
	}
public:
	ExampleDeserializerStream(std::istream &stream): DeserializerStream(stream){}
};
