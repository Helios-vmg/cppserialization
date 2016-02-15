#ifndef PARSER_H
#define PARSER_H

#ifndef HAVE_PRECOMPILED_HEADERS
#include <vector>
#include <memory>
#include <deque>
#endif

class IdentifierToken;
class Token;
class CppNonTerminal;
class CppFile;

class FileNonTerminal{
	std::vector<std::shared_ptr<CppNonTerminal>> cpps;
public:
	FileNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::vector<std::shared_ptr<CppFile>> evaluate_ast() const;
};

std::vector<char> read_file(const char *path);
std::deque<std::shared_ptr<Token>> tokenize(const std::vector<char> &input);

#endif
