#include "test1.generated.h"
#include "test1.generated.cpp"
#include "gen.h"
#include "util.h"
#include <chrono>

using namespace test1_types;

#define GEN(x) gen(dst.x, rng)

void gen(A &dst, std::mt19937 &rng){
	GEN(x0);
	GEN(x1);
	GEN(x2);
	GEN(x3);
	GEN(x4);
	GEN(x5);
	GEN(x6);
	GEN(x7);
	GEN(x8);
	GEN(x9);
	GEN(x10);
	GEN(x11);
	GEN(x12);
	GEN(x13);
	GEN(x14);
	GEN(x15);
	GEN(x16);
	GEN(x17);
	GEN(x18);
	GEN(x19);
	GEN(x20);
	GEN(x21);
	GEN(x22);
	GEN(x23);
}

#define DO_C(x) if (l.x != r.x) return false

bool A::operator==(const A &r) const{
	auto &l = *this;
	DO_C(x0);
	DO_C(x1);
	DO_C(x2);
	DO_C(x3);
	DO_C(x4);
	DO_C(x5);
	DO_C(x6);
	DO_C(x7);
	DO_C(x8);
	DO_C(x9);
	DO_C(x10);
	DO_C(x11);
	DO_C(x12);
	DO_C(x13);
	DO_C(x14);
	DO_C(x15);
	DO_C(x16);
	DO_C(x17);
	DO_C(x18);
	DO_C(x19);
	DO_C(x20);
	DO_C(x21);
	DO_C(x22);
	DO_C(x23);
	return true;
}

void test1(std::uint32_t seed){
	std::mt19937 rng(seed);
	double serialization_total = 0;
	double deserialization_total = 0;
	const int N = 10'000;
	for (int i = N; i--;){
		A a;
		gen(a, rng);
		auto t0 = std::chrono::high_resolution_clock::now();
		auto serialized = serialize(a);
		auto t1 = std::chrono::high_resolution_clock::now();
		auto b = deserialize<A>(serialized);
		auto t2 = std::chrono::high_resolution_clock::now();
		if (a != *b)
			throw std::runtime_error("test failed: incorrect serialization round trip");
		auto serialized2 = serialize(*b);
		if (serialized != serialized2)
			throw std::runtime_error("test failed: serialization instability");

		serialization_total += (double)std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count();
		deserialization_total += (double)std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
	}
	std::cout <<
		"Serialization time (avg):   " << serialization_total / N << " us\n"
		"Deserialization time (avg): " << deserialization_total / N << " us\n";
}
