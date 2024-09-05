#include "test2.generated.hpp"
#include "test2.generated.cpp"
#include "util.hpp"

using namespace test2_types;

void test2(std::uint32_t seed){
	Root root;
	root.nodes.push_back(std::make_unique<Node>());
	root.nodes.push_back(std::make_unique<Node>());
	root.nodes.push_back(std::make_unique<Node>());
	root.nodes.push_back(std::make_unique<Node>());
	{
		auto &A = *root.nodes[0];
		auto &B = *root.nodes[1];
		auto &C = *root.nodes[2];
		auto &D = *root.nodes[3];
		A.links.push_back(&B);
		A.links.push_back(&C);
		B.links.push_back(&D);
		C.links.push_back(&D);
		A.data = 100;
		B.data = 101;
		C.data = 102;
		D.data = 103;
		root.root = &A;
	}

	auto root2 = round_trip(root);

	test_assertion(root2->nodes.size() == 4, "wrong number of nodes");
	test_assertion(!!root2->root, "no root");
	test_assertion(root2->root->data = 100, "wrong value (1)");
	test_assertion(root2->root->links.size() == 2, "wrong number of links (1)");
	test_assertion(root2->root->links[0]->data == 101, "wrong value (2)");
	test_assertion(root2->root->links[1]->data == 102, "wrong value (3)");
	test_assertion(root2->root->links[0]->links.size() == 1, "wrong number of links (2)");
	test_assertion(root2->root->links[1]->links.size() == 1, "wrong number of links (3)");
	test_assertion(root2->root->links[0]->links[0]->data == 103, "wrong value (4)");
	test_assertion(root2->root->links[1]->links[0]->data == 103, "wrong value (5)");

	root2->root->links[0]->links[0]->data = 42;
	test_assertion(root2->root->links[1]->links[0]->data == 42, "wrong value (6)");
}
