#include "tests.h"
#include <random>
#include <cstdint>
#include <iostream>

typedef void (*test_f)(std::uint32_t);

void test1(std::uint32_t);
void test2(std::uint32_t);
void test3(std::uint32_t);
void test4(std::uint32_t);
void test5(std::uint32_t);
void test6(std::uint32_t);
void test7(std::uint32_t);

void run_tests(){
	std::random_device dev;
	static const test_f tests[] = {
		test1,
		test2,
		test3,
		test4,
		test5,
		test6,
		test7,
	};
	size_t test_no = 0;
	std::uint32_t seed = dev();
	for (auto f : tests){
		test_no++;
		try{
			f(seed);
		}catch (std::exception &e){
			std::cerr << "Test #" << test_no << " failed: " << e.what() << std::endl;
			break;
		}
		std::cout << "Test #" << test_no << " passed.\n";
	}
	std::cout << "All tests passed.\n";
}
