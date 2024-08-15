#include "test3.generated.h"
#include "test3.generated.cpp"
#include "util.h"
#include <random>

using namespace test3_types;

int B::foo() const{
	return 3;
}

int C::foo() const{
	return 5;
}

int D::sum() const{
	int ret = 0;
	for (auto &p : this->objects)
		ret += p->foo();
	return ret;
}

void test3(std::uint32_t seed){
	D d;
	std::mt19937 rng;
	for (int i = 1024; i--;){
		std::unique_ptr<A> p;
		if (rng() % 2)
			p = std::make_unique<B>();
		else
			p = std::make_unique<C>();
		d.objects.emplace_back(std::move(p));
	}
	auto expected = d.sum();

	auto d2 = round_trip(d);

	test_assertion(d2->sum() == expected, "wrong sum");
}
