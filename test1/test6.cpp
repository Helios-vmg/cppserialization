#include "test6.generated.hpp"
#include "test6.generated.cpp"
#include "util.hpp"

void test6(std::uint32_t){
	Object o;
	o.type = ObjectType::Int;
	auto deserialized = deserialize<Object>(serialize(o));

	test_assertion(o.type == deserialized->type, "failed check #1");
	std::array<const char *, 3> strings = get_enum_string(ObjectType::Int);
	test_assertion(!strcmp(strings[0], "Int"), "failed string check #1");
	test_assertion(strings[2] == nullptr, "failed string check #2");
	test_assertion(!strcmp(get_enum_string(ObjectType::String)[2], "str"), "failed string check #3");
}
