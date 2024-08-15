#include "test6.generated.h"
#include "test6.generated.cpp"
#include "util.h"

void test6(std::uint32_t){
	Object o;
	o.type = ObjectType::Int;
	auto deserialized = deserialize<Object>(serialize(o));

	test_assertion(o.type == deserialized->type, "failed check #1");
}
