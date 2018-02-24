#include "inheritance.generated.h"
#include <ExampleDeserializerStream.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <sstream>
#include <chrono>

inheritance_graph::~inheritance_graph(){}
A::~A(){}
B::~B(){}
C::~C(){}
void inheritance_graph::rollback_deserialization(){}
void A::rollback_deserialization(){}
void B::rollback_deserialization(){}
void C::rollback_deserialization(){}

template <typename T1, typename T2>
void cpp_assert(const T1 &expr, const T2 &expected, const char *name){
	if (expr == expected)
		return;
	throw std::runtime_error((std::string)"Assertion " + name + " failed");
}

class testA{
public:
	int a;
	virtual ~testA(){}
	void set_a(int a){
		this->a = a;
	}
};

class testB : public virtual testA{
public:
	virtual ~testB(){}
};

class testC : public virtual testA{
public:
	virtual ~testC(){}
};

class testD : public testB, public testC{
public:
	int b;
	virtual ~testD(){}
};

int main(){
	{
		auto p = std::make_shared<testD>();
		p->set_a(42);
	}
	try{
		auto c = std::make_shared<C>();
		auto g = std::make_shared<inheritance_graph>();
		auto p1 = c.get();
		auto p2 = static_cast<Serializable *>(static_cast<B *>(p1));
		auto p3 = static_cast<A *>(p1);
		auto p4 = static_cast<B *>(p1);
		g->nodesA.push_back(c);
		g->nodesB.push_back(c);

		g->nodesA.front()->data_a = "hello";
		g->nodesB.front()->data_b.push_back("hello");

		cpp_assert(c->data_a, "hello", "ASSERT1");
		cpp_assert(c->data_b.size(), 1, "ASSERT2");
		cpp_assert(c->data_b.front(), "hello", "ASSERT3");

		c->data_a.clear();
		c->data_b.front().clear();
		c->set_data_c(3.141592);

		cpp_assert(g->nodesA.front()->data_a, "", "ASSERT4");
		cpp_assert(g->nodesB.front()->data_b.size(), 1, "ASSERT5");
		cpp_assert(g->nodesB.front()->data_b.front(), "", "ASSERT6");

		std::stringstream stream;
		{
			SerializerStream ss(stream);
			ss.full_serialization(*g, true);
		}
		auto str = stream.str();
		stream.clear();
		stream.seekg(0);
		std::shared_ptr<inheritance_graph> g2;
		{
			ExampleDeserializerStream ds(stream);
			g2.reset(ds.full_deserialization<inheritance_graph>(true));
		}

		auto c2 = std::dynamic_pointer_cast<C>(g2->nodesA.front());

		g2->nodesA.front()->data_a = "hello";
		g2->nodesB.front()->data_b.push_back("hello");

		cpp_assert(c2->data_a, "hello", "ASSERT7");
		cpp_assert(c2->data_b.size(), 2, "ASSERT8");
		cpp_assert(c2->data_b.back(), "hello", "ASSERT9");

		c2->data_a.clear();
		c2->data_b.back().clear();

		cpp_assert(g2->nodesA.front()->data_a, "", "ASSERT10");
		cpp_assert(g2->nodesB.front()->data_b.size(), 2, "ASSERT11");
		cpp_assert(g2->nodesB.front()->data_b.back(), "", "ASSERT12");

	}catch (std::exception &e){
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
