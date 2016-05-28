#include "stdafx.h"
#include "las.h"
#include "util.h"
#include "parser.h"
#ifndef HAVE_PRECOMPILED_HEADERS
#include <unordered_map>
#include <sstream>
#include <functional>
#include <fstream>
#include <queue>
#endif

int main(int argc, char **argv){
	if (argc < 2)
		return -1;
	{
		std::random_device dev;
		rng.seed(dev());
	}
	std::vector<std::shared_ptr<CppFile> > cpps;
	{
		auto file = read_file(argv[1]);
		auto input = tokenize(file);
		FileNonTerminal cpp(input);
		cpps = cpp.evaluate_ast();
	}
	for (auto &cpp : cpps){
		cpp->generate_header();
		cpp->generate_source();
		cpp->generate_aux();
	}
}
