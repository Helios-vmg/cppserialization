#include "las.hpp"
#include "token.hpp"
#include "nonterminal.hpp"
#include <functional>
#include <fstream>
#include <queue>
#include <iostream>

std::string read_file(const char *path){
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	std::string ret;
	if (!file)
		return ret;
	ret.resize((size_t)file.tellg());
	file.seekg(0);
	file.read(ret.data(), ret.size());
	return ret;
}

std::deque<char> to_deque(const std::string &s){
	return { s.begin(), s.end() };
}

std::deque<std::shared_ptr<Token>> tokenize(const std::string &input){
	auto deque = to_deque(make_newlines_consistent(input));
	return tokenize(deque);
}

int main(int argc, char **argv){
	try{
		if (argc < 2)
			return -1;
		{
			std::random_device dev;
			rng.seed(dev());
		}
		EvaluationResult r;
		{
			auto input = tokenize(read_file(argv[1]));
			FileNonTerminal file(input);
			r = file.evaluate_ast();
		}
		r.generate();
	}catch (std::exception &e){
		std::cerr << "Exception caught: " << e.what() << std::endl;
		return -1;
	}catch (...){
		std::cerr << "Unknown exception caught.\n";
		return -1;
	}
}
