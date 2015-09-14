#include "random_graph.generated.h"
#include <random>
#include <set>
#include <algorithm>
#include <sstream>

graph::~graph(){
	for (auto c : this->nodes)
		delete c;
	this->nodes.clear();
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
	this->nodes.reserve(2048);
	while (this->nodes.size() < this->nodes.capacity()){
		std::unique_ptr<node> n(new node(this->nodes.size(), random_string(prng)));
		this->nodes.push_back(n.release());
	}
	std::uniform_int_distribution<size_t> dist(0, this->nodes.size() - 1);
	this->first = this->nodes[dist(prng)];
	std::vector<size_t> children;
	for (size_t i = this->nodes.size(); i--;)
		children.push_back(i);
	for (const auto &i : this->nodes){
		std::shuffle(children.begin(), children.end(), prng);
		for (auto c = dist(prng); c--;)
			i->add_child(this->nodes[children[c]]);
	}
}

void graph::rollback_deserialization(){
}

std::string graph::stringize() const{
	std::string ret;
	ret += this->first->stringize();
	ret += '{';
	for (auto &n : this->nodes)
		ret += n->stringize();
	ret += '}';
	return ret;
}

//------------------------------------------------------------------------------

node::~node(){
}

void node::rollback_deserialization(){
}

std::string node::stringize() const{
	std::stringstream ret;
	ret << "(" << this->name << ":" << this->data << "{";
	for (auto &c : this->children)
		ret << c->name << ",";
	ret << "})";
	return ret.str();
}
