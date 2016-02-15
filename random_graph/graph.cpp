#include "random_graph.generated.h"
#include <random>
#include <set>
#include <algorithm>
#include <sstream>

graph::~graph(){
	for (auto node : this->get_all_nodes_unordered((size_t)this->node_count))
		delete node;
}

template <typename T>
void graph::get_all_nodes_generic(T &dst, size_t known_max) const{
	std::vector<node *> stack;
	stack.push_back(this->first);
	dst.insert(this->first);
	decltype(stack) temp_stack;
	while (stack.size()){
		auto top = stack.back();
		stack.pop_back();
		top->get_children(temp_stack);
		for (auto &i : temp_stack){
			if (dst.find(i) != dst.end())
				continue;
			dst.insert(i);
			stack.push_back(i);
		}
		temp_stack.clear();
		if (dst.size() == known_max)
			break;
	}
}

std::unordered_set<node *> graph::get_all_nodes_unordered(size_t known_max) const{
	std::unordered_set<node *> ret;
	this->get_all_nodes_generic(ret, known_max);
	return ret;
}

std::set<node *> graph::get_all_nodes(size_t known_max) const{
	std::set<node *> ret;
	this->get_all_nodes_generic(ret, known_max);
	return ret;
}

std::string random_string(std::mt19937 &prng){
	std::string ret;
	std::uniform_int_distribution<unsigned> dist('a', 'z');
	{
		std::uniform_int_distribution<size_t> dist2(2, 32);
		ret.resize(dist2(prng));
	}
	for (auto &c : ret)
		c = (char)dist(prng);
	return ret;
}

void graph::generate(unsigned seed){
	std::mt19937 prng;
	prng.seed(seed);
	std::vector<node *> nodes;
	nodes.reserve(1024*1024);
	std::cout << "Generating vertices...\n";
	while (nodes.size() < nodes.capacity())
		nodes.push_back(new node(nodes.size(), random_string(prng)));
	{
		std::uniform_int_distribution<size_t> dist(0, nodes.size() - 1);
		this->first = nodes[dist(prng)];
	}
	std::cout << "Generating edges...\n";
	std::vector<size_t> children;
	for (size_t i = nodes.size(); i--;)
		children.push_back(i);
	{
		std::uniform_int_distribution<size_t> fulldist(0, nodes.size() - 1);
		const size_t max = std::min<size_t>(nodes.size() - 1, 1024);
		for (const auto &i : nodes){
			std::uniform_int_distribution<size_t> dist(0, max);
			auto n = dist(prng);
			if (!n)
				continue;
			for (size_t j = 0; j < n - 1; j++){
				std::uniform_int_distribution<size_t> temp(j, nodes.size() - 1);
				auto pick = temp(prng);
				std::swap(children[j], children[pick]);
			}
			for (auto c = n; c--;)
				i->add_child(nodes[children[c]]);
		}
	}
	std::cout << "Deleting unlinked nodes...\n";
	std::set<node *> nodes2(nodes.begin(), nodes.end());
	auto all_nodes = this->get_all_nodes(nodes.size());
	this->node_count = all_nodes.size();
	std::set<node *> unused_nodes;
	std::set_difference(nodes2.begin(), nodes2.end(), all_nodes.begin(), all_nodes.end(), std::inserter(unused_nodes, unused_nodes.begin()));
	for (auto node : unused_nodes)
		delete node;
}

void graph::rollback_deserialization(){
}

void graph::stringize(std::ostream &stream) const{
	auto all_nodes = this->get_all_nodes_unordered((size_t)this->node_count);
	std::vector<node *> sorted_nodes(all_nodes.begin(), all_nodes.end());
	std::sort(sorted_nodes.begin(), sorted_nodes.end(), [](node *a, node *b){ return a->get_name() < b->get_name(); });
	for (auto node : sorted_nodes)
		node->stringize(stream);
	stream << "first: " << this->first->get_name();
}

//------------------------------------------------------------------------------

node::~node(){
}

void node::rollback_deserialization(){
}

void node::stringize(std::ostream &stream) const{
	stream << "(" << this->name << ":" << this->data << "{";
	for (auto &c : this->children)
		stream << c->name << ",";
	stream << "})\n";
}
