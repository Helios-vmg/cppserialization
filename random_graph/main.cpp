#include "random_graph.generated.h"
#include <iostream>
#include <iomanip>
#include <fstream>

int main(){
	{
		graph g;
		g.generate(42);
		{
			std::ofstream file("random_graph.bin", std::ios::binary);
			SerializerStream ss(file);
			ss.begin_serialization(g, true);
		}
		std::ofstream file("random_graph.txt");
		file << g.stringize() << std::endl;
	}
	{
		std::unique_ptr<graph> g;
		{
			std::ifstream file("random_graph.bin", std::ios::binary);
			DeserializerStream ds(file);
			g.reset(ds.begin_deserialization<graph>(true));
			if (!g){
				std::cerr << "Deserialization failed!\n";
				return 0;
			}
		}
		std::ofstream file("random_graph.restored.txt");
		file << g->stringize() << std::endl;
	}
	return 0;
}
