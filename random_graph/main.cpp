#include "random_graph.generated.h"
#include <ExampleDeserializerStream.h>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <chrono>

int main(){
	{
		graph graph;
		std::cout << "Generating graph...\n";
		graph.generate(42);
		{
			std::ofstream file("random_graph.bin", std::ios::binary);

			SerializerStream ss(file);
			auto t0 = std::chrono::high_resolution_clock::now();
			try{
				ss.full_serialization(graph, true);
			}catch (std::exception &){
				return 0;
			}
			auto t1 = std::chrono::high_resolution_clock::now();

			auto duration = std::chrono::duration <double, std::milli>(t1 - t0).count();
			std::cout << "Serialization took " << duration << " ms (approx. " << duration / graph.get_node_count() * 1000 << " us/node)\n";
		}
		std::ofstream file("random_graph.txt");
		graph.stringize(file);
	}
	try{
		std::unique_ptr<graph> graph;
		{
			std::ifstream file("random_graph.bin", std::ios::binary);

			ExampleDeserializerStream ds(file);
			auto t0 = std::chrono::high_resolution_clock::now();
			graph.reset(ds.full_deserialization<::graph>(true));
			auto t1 = std::chrono::high_resolution_clock::now();

			if (!graph){
				std::cerr << "Deserialization failed!\n";
				return 0;
			}
			auto duration = std::chrono::duration <double, std::milli>(t1 - t0).count();
			std::cout << "Deserialization took " << duration << " ms (approx. " << duration / graph->get_node_count() * 1000 << " us/node)\n";
		}
		std::ofstream file("random_graph.restored.txt");
		graph->stringize(file);
	}catch (std::exception &e){
		std::cerr << e.what() << std::endl;
	}
	return 0;
}
