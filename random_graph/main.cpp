#include "random_graph.generated.h"
#include <iostream>
#include <iomanip>
#include <fstream>

#define RESTORE

int main(){
#ifndef RESTORE
	graph g;
	g.generate(42);
	{
		std::ofstream file("random_graph.bin", std::ios::binary);
		SerializerStream ss(file);
		ss.begin_serialization(g, true);
	}
	{
		std::ofstream file("random_graph.txt");
		file << g.stringize() << std::endl;
	}
#else
	graph *g;
	{
		std::ifstream file("random_graph.bin", std::ios::binary);
		DeserializerStream ds(file);
		g = ds.begin_deserialization<graph>(true);
	}
	{
		std::ofstream file("random_graph.restored.txt");
		file << g->stringize() << std::endl;
	}
	delete g;
#endif
}
