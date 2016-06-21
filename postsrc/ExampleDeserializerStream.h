#include "DeserializerStream.h"

class DeserializationException : public std::exception{
protected:
	const char *message;
public:
	DeserializationException(DeserializerStream::ErrorType type){
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
			default:
				this->message = "DeserializationError: Unknown.";
				break;
		}
	}
	virtual const char *what() const noexcept override{
		return this->message;
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
