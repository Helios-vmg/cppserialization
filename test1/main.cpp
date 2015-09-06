#include "testtype.generated.h"
#include <fstream>

int main(){
	{
		test_type a[3];
		a[0].valid = true;
		a[1].valid = true;
		a[2].valid = true;
		a[0].name = "hello0";
		a[1].name = "hello1";
		a[2].name = "hello2";
		a[0].next = a + 1;
		a[1].next = a + 2;
		a[2].next = a + 0;

		std::ofstream file("test.bin", std::ios::binary);
		SerializerStream ss(file);
		ss.begin_serialization(a[0], true);
	}
	{
		std::ifstream file("test.bin", std::ios::binary);
		DeserializerStream ds(file);
		std::unique_ptr<test_type> tt(ds.begin_deserialization<test_type>(true));
	}
}
