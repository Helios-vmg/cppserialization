#include "test6.generated.hpp"
#include "test6.generated.cpp"
#include "util.hpp"

void test6(std::uint32_t){
	Object o;
	o.type = ObjectType::Int;
	auto deserialized = deserialize<Object>(serialize(o));

	test_assertion(o.type == deserialized->type, "failed check #1");
}
