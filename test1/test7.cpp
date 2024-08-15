#include "test7_a.generated.h"
#include "test7_a.generated.h"
#include "test7_b.generated.h"
#include "util.h"
#include <optional>

void test7(std::uint32_t){
	A::Object o1;
	o1.type = A::ObjectType::Int;
	auto o2 = deserialize<B::Object>(serialize(o1));
	test_assertion((std::uint32_t)o1.type == (std::uint32_t)o2->type, "failed check #1");

	o1.type = A::ObjectType::Double;

	std::optional<DeserializerStream::ErrorType> error;
	try{
		o2 = deserialize<B::Object>(serialize(o1));
	}catch (DeserializationException &e){
		error = e.get_type();
	}
	test_assertion(error.has_value() && *error == DeserializerStream::ErrorType::UnknownEnumValue, "failed check #2");
}
