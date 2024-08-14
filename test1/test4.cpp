#include "test4.generated.h"
#include "test4.generated.cpp"
#include "test4.aux.generated.cpp"
#include "util.h"

void test4(std::uint32_t){
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

	test_assertion(c->data_a == "hello", "ASSERT1");
	test_assertion(c->data_b.size() == 1, "ASSERT2");
	test_assertion(c->data_b.front() == "hello", "ASSERT3");

	c->data_a.clear();
	c->data_b.front().clear();
	c->set_data_c(3.141592);

	test_assertion(g->nodesA.front()->data_a.empty(), "ASSERT4");
	test_assertion(g->nodesB.front()->data_b.size() == 1, "ASSERT5");
	test_assertion(g->nodesB.front()->data_b.front().empty(), "ASSERT6");

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
		g2 = ds.full_deserialization<inheritance_graph>(true);
	}

	auto c2 = std::dynamic_pointer_cast<C>(g2->nodesA.front());

	g2->nodesA.front()->data_a = "hello";
	g2->nodesB.front()->data_b.push_back("hello");

	test_assertion(c2->data_a == "hello", "ASSERT7");
	test_assertion(c2->data_b.size() == 2, "ASSERT8");
	test_assertion(c2->data_b.back() == "hello", "ASSERT9");

	c2->data_a.clear();
	c2->data_b.back().clear();

	test_assertion(g2->nodesA.front()->data_a.empty(), "ASSERT10");
	test_assertion(g2->nodesB.front()->data_b.size() == 2, "ASSERT11");
	test_assertion(g2->nodesB.front()->data_b.back().empty(), "ASSERT12");
}
