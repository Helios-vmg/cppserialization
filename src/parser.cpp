#include "stdafx.h"
#include "parser.h"
#include "las.h"
#include "GenericException.h"
#include <fstream>
#include <cassert>

//------------------------------------------------------------------------------

class TypeDeclarationNonTerminal;

//------------------------------------------------------------------------------

const int first_name_token = 1000;

const char * const keywords[] = {
	"cpp",
	"struct",
	"class",
	"public",
	"private",
	"protected",
	"pointer",
	"shared_ptr",
	"unique_ptr",
	"vector",
	"list",
	"set",
	"map",
	"include",
	"bool",
	"uint8_t",
	"uint16_t",
	"uint32_t",
	"uint64_t",
	"int8_t",
	"int16_t",
	"int32_t",
	"int64_t",
	"float",
	"double",
	"unordered_set",
	"unordered_map",
	"string",
	"wstring",
	"abstract",
	nullptr
};

const char * const whitespace = " \f\t\r\n";
const char * const simple_tokens = "<>{}[]:;#,";

std::unordered_map<std::string, std::shared_ptr<Type>> dsl_type_map;

//------------------------------------------------------------------------------

enum class TokenType{
	FixedToken,
	IdentifierToken,
	IntegerToken,
	StringLiteralToken,
};

enum class FixedTokenType{
	LBrace        = '{',
	RBrace        = '}',
	LAngleBracket = '<',
	RAngleBracket = '>',
	LBracket      = '[',
	RBracket      = ']',
	Colon         = ':',
	Semicolon     = ';',
	Hash          = '#',
	Comma         = ',',
	Cpp           = first_name_token + 0,
	Struct        = first_name_token + 1,
	Class         = first_name_token + 2,
	Public        = first_name_token + 3,
	Private       = first_name_token + 4,
	Protected     = first_name_token + 5,
	Pointer       = first_name_token + 6,
	SharedPtr     = first_name_token + 7,
	UniquePtr     = first_name_token + 8,
	Vector        = first_name_token + 9,
	List          = first_name_token + 10,
	Set           = first_name_token + 11,
	Map           = first_name_token + 12,
	Include       = first_name_token + 13,
	Bool          = first_name_token + 14,
	Uint8T        = first_name_token + 15,
	Uint16T       = first_name_token + 16,
	Uint32T       = first_name_token + 17,
	Uint64T       = first_name_token + 18,
	Int8T         = first_name_token + 19,
	Int16T        = first_name_token + 20,
	Int32T        = first_name_token + 21,
	Int64T        = first_name_token + 22,
	Float         = first_name_token + 23,
	Double        = first_name_token + 24,
	UnorderedSet  = first_name_token + 25,
	UnorderedMap  = first_name_token + 26,
	String        = first_name_token + 27,
	WString       = first_name_token + 28,
	Abstract      = first_name_token + 29,
};

enum class AccessType{
	Public,
	Protected,
	Private,
};

//------------------------------------------------------------------------------

class ParsingError : public std::exception{
	std::string message;
public:
	ParsingError(){}
	template <typename T>
	ParsingError &operator<<(const T &o){
		std::stringstream stream;
		stream << o;
		this->message += stream.str();
		return *this;
	}
	const char *what() const NOEXCEPT override{
		return this->message.c_str();
	}
};

class Token{
public:
	virtual ~Token(){}
	virtual TokenType token_type() const = 0;
};

class FixedToken : public Token{
	FixedTokenType type;
public:
	FixedToken(FixedTokenType type): type(type){}
	FixedTokenType get_type() const{
		return this->type;
	}
	TokenType token_type() const override{
		return TokenType::FixedToken;
	}
};

class IdentifierToken : public Token{
	std::string name;
public:
	IdentifierToken(const std::string &name): name(name){}
	const std::string &get_name() const{
		return this->name;
	}
	TokenType token_type() const override{
		return TokenType::IdentifierToken;
	}
};

class IntegerToken : public Token {
	unsigned value;
public:
	IntegerToken(unsigned value): value(value){}
	unsigned get_value() const{
		return this->value;
	}
	TokenType token_type() const override{
		return TokenType::IntegerToken;
	}
};

class StringLiteralToken : public Token{
	std::string value;
public:
	StringLiteralToken(const std::string &value): value(value){}
	const std::string &get_value() const{
		return this->value;
	}
	TokenType token_type() const override{
		return TokenType::StringLiteralToken;
	}
};

class TypeSpecificationNonTerminal{
public:
	virtual ~TypeSpecificationNonTerminal() = 0;

	virtual std::shared_ptr<Type> create_type() const = 0;
	static std::shared_ptr<TypeSpecificationNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
};

class UserTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	std::shared_ptr<IdentifierToken> name;
public:
	UserTypeSpecificationNonTerminal(const std::shared_ptr<IdentifierToken> &name): name(name){}
	std::shared_ptr<Type> create_type() const override;
};

class NullaryTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	FixedTokenType type;
public:
	NullaryTypeSpecificationNonTerminal(const FixedTokenType &type): type(type){}
	std::shared_ptr<Type> create_type() const override;
};

class UnaryTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	FixedTokenType outer_type;
	std::shared_ptr<TypeSpecificationNonTerminal> inner_type;
public:
	UnaryTypeSpecificationNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::shared_ptr<Type> create_type() const override;
};

class BinaryTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	FixedTokenType outer_type;
	std::shared_ptr<TypeSpecificationNonTerminal> inner_types[2];
public:
	BinaryTypeSpecificationNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::shared_ptr<Type> create_type() const override;
};

class ClassInternalNonTerminal{
public:
	virtual ~ClassInternalNonTerminal() = 0;
	static std::shared_ptr<ClassInternalNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
	virtual void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility) const = 0;
};

class AccessSpecifierNonTerminal : public ClassInternalNonTerminal{
	AccessType access;
public:
	AccessSpecifierNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility) const override;
};

class IncludeDirectiveNonTerminal : public ClassInternalNonTerminal{
	std::shared_ptr<StringLiteralToken> path;
	bool relative;
public:
	IncludeDirectiveNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility) const override{
		Class->add_element(std::make_shared<UserInclude>(Class->get_root(), this->path->get_value(), this->relative));
	}
};

class DataDeclarationNonTerminal : public ClassInternalNonTerminal{
	std::shared_ptr<TypeSpecificationNonTerminal> type;
	std::shared_ptr<IdentifierToken> name;
	unsigned length;
public:
	DataDeclarationNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility) const override;
};

class TypeDeclarationNonTerminal{
public:
	virtual ~TypeDeclarationNonTerminal() = 0;
	static std::shared_ptr<TypeDeclarationNonTerminal> create(std::deque<std::shared_ptr<Token>> &input, bool assume_abstract = false);
	virtual std::shared_ptr<CppElement> initial_declaration(CppFile &) const = 0;
	virtual void finish_declaration(const std::shared_ptr<CppElement> &) const = 0;
};

class ClassNonTerminal : public TypeDeclarationNonTerminal{
	bool public_default;
	bool abstract;
	std::shared_ptr<IdentifierToken> name;
	std::vector<std::shared_ptr<IdentifierToken>> base_classes;
	std::vector<std::shared_ptr<ClassInternalNonTerminal>> internals;
public:
	ClassNonTerminal(std::deque<std::shared_ptr<Token>> &input, bool public_default, bool is_abstract = false);
	std::shared_ptr<CppElement> initial_declaration(CppFile &file) const override{
		return std::make_shared<UserClass>(file, this->name->get_name(), this->abstract);
	}
	void finish_declaration(const std::shared_ptr<CppElement> &) const;
};

class CppNonTerminal{
	std::shared_ptr<IdentifierToken> name;
	std::vector<std::shared_ptr<TypeDeclarationNonTerminal>> declarations;
public:
	CppNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::shared_ptr<CppFile> evaluate_ast() const;
};

//------------------------------------------------------------------------------

std::vector<char> read_file(const char *path){
	std::ifstream file(path, std::ios::binary|std::ios::ate);
	std::vector<char> ret;
	if (!file)
		return ret;
	ret.resize((size_t)file.tellg());
	file.seekg(0);
	file.read(&ret[0], ret.size());
	return ret;
}

template <typename T>
std::deque<T> to_deque(const std::vector<T> &vector){
	return std::deque<T>(vector.begin(), vector.end());
}

template <typename T>
bool char_matches_any(T c, const T *p){
	for (;*p; p++)
		if (c == *p)
			return true;
	return false;
}

bool is_whitespace(char c){
	return char_matches_any(c, whitespace);
}

bool is_simple_token(char c){
	return char_matches_any(c, simple_tokens);
}

bool is_first_identifier_char(char c){
	c = tolower(c);
	return c == '_' || c >= 'a' && c <= 'z';
}

bool is_nth_identifier_char(char c){
	return is_first_identifier_char(c) || c >= '0' && c <= '9';
}

std::shared_ptr<IdentifierToken> tokenize_identifier(std::deque<char> &input){
	assert(input.size() && is_first_identifier_char(input.front()));
	std::string identifier;
	identifier.push_back(input.front());
	input.pop_front();
	while (input.size()){
		auto c = input.front();
		if (!is_nth_identifier_char(c)) 
			break;
		identifier.push_back(c);
		input.pop_front();
	}
	return std::make_shared<IdentifierToken>(identifier);
}

std::deque<std::shared_ptr<Token>> tokenize(std::deque<char> &input){
	std::deque<std::shared_ptr<Token>> ret;
	bool seen_first_comment_char = false;
	bool in_comment = false;
	while (input.size()){
		auto c = input.front();
		if (in_comment){
			if (seen_first_comment_char && c == '/'){
				seen_first_comment_char = false;
				in_comment = false;
				input.pop_front();
				continue;
			}
			seen_first_comment_char = c == '*';
			input.pop_front();
			continue;
		}
		if (seen_first_comment_char){
			if (c == '*'){
				seen_first_comment_char = false;
				in_comment = true;
				input.pop_front();
				continue;
			}
			throw GenericException();
		}
		if (c == '/'){
			seen_first_comment_char = true;
			input.pop_front();
			continue;
		}
		if (is_whitespace(c)) {
			input.pop_front();
			continue;
		}
		if (is_simple_token(c)){
			ret.push_back(std::make_shared<FixedToken>((FixedTokenType)c));
			input.pop_front();
			continue;
		}
		if (is_first_identifier_char(c)){
			auto token = tokenize_identifier(input);
			const auto &name = token->get_name();
			bool found = false;
			for (size_t i = 0; keywords[i]; i++){
				if (name == keywords[i]){
					ret.push_back(std::make_shared<FixedToken>((FixedTokenType)(first_name_token + i)));
					found = true;
					break;
				}
			}
			if (found)
				continue;
			ret.push_back(token);
			continue;
		}
		if (c == '"'){
			input.pop_front();
			std::string string;
			while (input.size()){
				if (input.front() == '"'){
					input.pop_front();
					break;
				}
				string.push_back(input.front());
				input.pop_front();
			}
			ret.push_back(std::make_shared<StringLiteralToken>(string));
			continue;
		}
		if (isdigit(c)){
			unsigned integer = 0;
			while (input.size()){
				auto c = input.front();
				if (!isdigit(c))
					break;
				integer *= 10;
				integer += c - '0';
				input.pop_front();
			}
			ret.push_back(std::make_shared<IntegerToken>(integer));
			continue;
		}
		throw GenericException();
	}
	return ret;
}

std::deque<std::shared_ptr<Token>> tokenize(const std::vector<char> &input){
	auto deque = to_deque(input);
	return tokenize(deque);
}

void require_token_type(std::deque<std::shared_ptr<Token>> &input, TokenType type){
	if (!input.size())
		throw ParsingError();
	if (input.front()->token_type() != type)
		throw ParsingError();
}

void require_fixed_token(std::deque<std::shared_ptr<Token>> &input, FixedTokenType type){
	require_token_type(input, TokenType::FixedToken);
	if (std::static_pointer_cast<FixedToken>(input.front())->get_type() != type)
		throw ParsingError();
}

FileNonTerminal::FileNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	while (input.size())
		this->cpps.push_back(std::make_shared<CppNonTerminal>(input));
}

CppNonTerminal::CppNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Cpp);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::LBrace);
	input.pop_front();

	while (input.size()){
		if (input.front()->token_type() == TokenType::FixedToken && std::static_pointer_cast<FixedToken>(input.front())->get_type() == FixedTokenType::RBrace)
			break;
		this->declarations.push_back(TypeDeclarationNonTerminal::create(input));
	}
	input.pop_front();
}

TypeDeclarationNonTerminal::~TypeDeclarationNonTerminal(){}

std::shared_ptr<TypeDeclarationNonTerminal> TypeDeclarationNonTerminal::create(std::deque<std::shared_ptr<Token>> &input, bool assume_abstract){
	require_token_type(input, TokenType::FixedToken);
	auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
	switch (type) {
		case FixedTokenType::Abstract:
			if (assume_abstract)
				throw ParsingError();
			input.pop_front();
			return TypeDeclarationNonTerminal::create(input, true);
		case FixedTokenType::Struct:
			return std::make_shared<ClassNonTerminal>(input, true, assume_abstract);
		case FixedTokenType::Class:
			return std::make_shared<ClassNonTerminal>(input, false, assume_abstract);
		default:
			throw ParsingError();
	}
}

ClassNonTerminal::ClassNonTerminal(std::deque<std::shared_ptr<Token>> &input, bool public_default, bool is_abstract){
	this->abstract = is_abstract;
	input.pop_front();
	this->public_default = public_default;
	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_token_type(input, TokenType::FixedToken);
	if (std::static_pointer_cast<FixedToken>(input.front())->get_type() == FixedTokenType::Colon){
		input.pop_front();

		while (true){
			require_token_type(input, TokenType::IdentifierToken);
			this->base_classes.push_back(std::static_pointer_cast<IdentifierToken>(input.front()));
			input.pop_front();

			require_token_type(input, TokenType::FixedToken);
			if (std::static_pointer_cast<FixedToken>(input.front())->get_type() != FixedTokenType::Comma)
				break;
			input.pop_front();
		}
	}

	require_fixed_token(input, FixedTokenType::LBrace);
	input.pop_front();

	while (input.size()){
		if (input.front()->token_type() == TokenType::FixedToken && std::static_pointer_cast<FixedToken>(input.front())->get_type() == FixedTokenType::RBrace)
			break;
		this->internals.push_back(ClassInternalNonTerminal::create(input));
	}
	input.pop_front();
}

bool is_typename_token(FixedTokenType type){
	switch (type){
		case FixedTokenType::Pointer:
		case FixedTokenType::SharedPtr:
		case FixedTokenType::UniquePtr:
		case FixedTokenType::Vector:
		case FixedTokenType::List:
		case FixedTokenType::Set:
		case FixedTokenType::Map:
		case FixedTokenType::Bool:
		case FixedTokenType::Uint8T:
		case FixedTokenType::Uint16T:
		case FixedTokenType::Uint32T:
		case FixedTokenType::Uint64T:
		case FixedTokenType::Int8T:
		case FixedTokenType::Int16T:
		case FixedTokenType::Int32T:
		case FixedTokenType::Int64T:
		case FixedTokenType::Float:
		case FixedTokenType::Double:
		case FixedTokenType::UnorderedSet:
		case FixedTokenType::UnorderedMap:
		case FixedTokenType::String:
		case FixedTokenType::WString:
			return true;
	}
	return false;
}

bool is_access_specifier_token(FixedTokenType type){
	switch (type){
		case FixedTokenType::Public:
		case FixedTokenType::Private:
		case FixedTokenType::Protected:
			return true;
	}
	return false;
}

ClassInternalNonTerminal::~ClassInternalNonTerminal(){}

std::shared_ptr<ClassInternalNonTerminal> ClassInternalNonTerminal::create(std::deque<std::shared_ptr<Token>> &input){
	if (!input.size())
		throw ParsingError();
	if (input.front()->token_type() == TokenType::FixedToken) {
		auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
		if (type == FixedTokenType::Hash)
			return std::make_shared<IncludeDirectiveNonTerminal>(input);
		if (is_typename_token(type))
			return std::make_shared<DataDeclarationNonTerminal>(input);
		if (is_access_specifier_token(type))
			return std::make_shared<AccessSpecifierNonTerminal>(input);
		throw ParsingError();
	}
	if (input.front()->token_type() == TokenType::IdentifierToken)
		return std::make_shared<DataDeclarationNonTerminal>(input);
	throw ParsingError();
}

AccessSpecifierNonTerminal::AccessSpecifierNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_token_type(input, TokenType::FixedToken);
	auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
	switch (type){
		case FixedTokenType::Public:
			this->access = AccessType::Public;
			break;
		case FixedTokenType::Private:
			this->access = AccessType::Private;
			break;
		case FixedTokenType::Protected:
			this->access = AccessType::Protected;
			break;
		default:
			throw ParsingError();
	}
	input.pop_front();

	require_fixed_token(input, FixedTokenType::Colon);
	input.pop_front();
}

IncludeDirectiveNonTerminal::IncludeDirectiveNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Hash);
	input.pop_front();

	require_fixed_token(input, FixedTokenType::Include);
	input.pop_front();

	if (!input.size())
		throw ParsingError();
	this->relative = input.front()->token_type() == TokenType::StringLiteralToken;
	if (this->relative){
		this->path = std::static_pointer_cast<StringLiteralToken>(input.front());
		input.pop_front();
	}else{
		require_fixed_token(input, FixedTokenType::LAngleBracket);
		input.pop_front();

		require_token_type(input, TokenType::StringLiteralToken);
		this->path = std::static_pointer_cast<StringLiteralToken>(input.front());
		input.pop_front();

		require_fixed_token(input, FixedTokenType::RAngleBracket);
		input.pop_front();
	}
}

DataDeclarationNonTerminal::DataDeclarationNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	if (!input.size())
		throw ParsingError();
	if (input.front()->token_type() == TokenType::FixedToken) {
		auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
		if (!is_typename_token(type))
			throw ParsingError();
	}
	this->type = TypeSpecificationNonTerminal::create(input);

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_token_type(input, TokenType::FixedToken);
	auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
	this->length = 1;
	if (type == FixedTokenType::LBracket){
		input.pop_front();
			
		require_token_type(input, TokenType::IntegerToken);
		this->length = std::static_pointer_cast<IntegerToken>(input.front())->get_value();
		input.pop_front();

		require_fixed_token(input, FixedTokenType::RBracket);
		input.pop_front();
	}

	require_fixed_token(input, FixedTokenType::Semicolon);
	input.pop_front();
}

TypeSpecificationNonTerminal::~TypeSpecificationNonTerminal(){}

std::shared_ptr<TypeSpecificationNonTerminal> TypeSpecificationNonTerminal::create(std::deque<std::shared_ptr<Token>> &input){
	if (!input.size())
		throw ParsingError();
	if (input.front()->token_type() == TokenType::IdentifierToken){
		auto ret = std::make_shared<UserTypeSpecificationNonTerminal>(std::static_pointer_cast<IdentifierToken>(input.front()));
		input.pop_front();
		return ret;
	}
	require_token_type(input, TokenType::FixedToken);
	auto token = std::static_pointer_cast<FixedToken>(input.front());
	auto type = token->get_type();
	switch (type){
		case FixedTokenType::Bool:
		case FixedTokenType::Uint8T:
		case FixedTokenType::Uint16T:
		case FixedTokenType::Uint32T:
		case FixedTokenType::Uint64T:
		case FixedTokenType::Int8T:
		case FixedTokenType::Int16T:
		case FixedTokenType::Int32T:
		case FixedTokenType::Int64T:
		case FixedTokenType::Float:
		case FixedTokenType::Double:
		case FixedTokenType::String:
		case FixedTokenType::WString:
			input.pop_front();
			return std::make_shared<NullaryTypeSpecificationNonTerminal>(type);
		case FixedTokenType::Pointer:
		case FixedTokenType::SharedPtr:
		case FixedTokenType::UniquePtr:
		case FixedTokenType::Vector:
		case FixedTokenType::List:
		case FixedTokenType::Set:
		case FixedTokenType::UnorderedSet:
			return std::make_shared<UnaryTypeSpecificationNonTerminal>(input);
		case FixedTokenType::Map:
		case FixedTokenType::UnorderedMap:
			return std::make_shared<BinaryTypeSpecificationNonTerminal>(input);
		default:
			throw ParsingError();
	}
}

UnaryTypeSpecificationNonTerminal::UnaryTypeSpecificationNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_token_type(input, TokenType::FixedToken);
	auto token = std::static_pointer_cast<FixedToken>(input.front());
	this->outer_type = token->get_type();
	input.pop_front();
		
	require_fixed_token(input, FixedTokenType::LAngleBracket);
	input.pop_front();

	this->inner_type = TypeSpecificationNonTerminal::create(input);

	require_fixed_token(input, FixedTokenType::RAngleBracket);
	input.pop_front();
}

BinaryTypeSpecificationNonTerminal::BinaryTypeSpecificationNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_token_type(input, TokenType::FixedToken);
	auto token = std::static_pointer_cast<FixedToken>(input.front());
	this->outer_type = token->get_type();
	input.pop_front();
		
	require_fixed_token(input, FixedTokenType::LAngleBracket);
	input.pop_front();

	this->inner_types[0] = TypeSpecificationNonTerminal::create(input);

	require_fixed_token(input, FixedTokenType::Comma);
	input.pop_front();

	this->inner_types[1] = TypeSpecificationNonTerminal::create(input);

	require_fixed_token(input, FixedTokenType::RAngleBracket);
	input.pop_front();
}

std::vector<std::shared_ptr<CppFile>> FileNonTerminal::evaluate_ast() const{
	std::vector<std::shared_ptr<CppFile>> ret;
	for (auto &cpp : this->cpps)
		ret.push_back(cpp->evaluate_ast());
	return ret;
}

std::shared_ptr<CppFile> CppNonTerminal::evaluate_ast() const{
	std::deque<std::shared_ptr<CppElement>> queue;
	auto ret = std::make_shared<CppFile>(this->name->get_name());

	for (auto &declaration : this->declarations){
		auto element = declaration->initial_declaration(*ret);
		auto type = std::dynamic_pointer_cast<Type>(element);
		if (type)
			dsl_type_map[type->get_type_string()] = type;
		queue.push_back(element);
	}

	for (auto &declaration : this->declarations){
		declaration->finish_declaration(queue.front());
		ret->add_element(queue.front());
		queue.pop_front();
	}

	for (auto &declaration : this->declarations){
		auto uc = std::dynamic_pointer_cast<UserClass>(declaration);
		if (!uc)
			continue;
		uc->mark_virtual_base_classes();
	}

	return ret;
}

void ClassNonTerminal::finish_declaration(const std::shared_ptr<CppElement> &element) const{
	auto Class = std::dynamic_pointer_cast<UserClass>(element);

	if (!Class)
		throw GenericException("Internal error: program in unknown state!");

	for (auto &b : this->base_classes){
		auto it = dsl_type_map.find(b->get_name());
		if (it == dsl_type_map.end())
			continue;
		auto type = std::dynamic_pointer_cast<UserClass>(it->second);
		Class->add_base_class(type);
	}

	auto current_accessibility = this->public_default ? Accessibility::Public : Accessibility::Private;
	for (auto &i : this->internals){
		i->modify_class(Class, current_accessibility);
	}
}

void AccessSpecifierNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility) const{
	switch (this->access) {
		case AccessType::Public:
			current_accessibility = Accessibility::Public;
			break;
		case AccessType::Protected:
			current_accessibility = Accessibility::Protected;
			break;
		case AccessType::Private:
			current_accessibility = Accessibility::Private;
			break;
		default:
			throw GenericException();
	}
}

void DataDeclarationNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility) const{
	auto type = this->type->create_type();
	if (this->length > 1)
		type = std::make_shared<ArrayType>(type, this->length);
	auto name = this->name->get_name();
	auto member = std::make_shared<ClassMember>(type, name, current_accessibility);
	Class->add_element(member);
}

std::shared_ptr<Type> UserTypeSpecificationNonTerminal::create_type() const{
	auto name = this->name->get_name();
	auto it = dsl_type_map.find(name);
	if (it == dsl_type_map.end())
		throw ParsingError();

	return it->second;
}

std::shared_ptr<Type> NullaryTypeSpecificationNonTerminal::create_type() const{
	switch (this->type){
		case FixedTokenType::Bool:
			return std::make_shared<BoolType>();
		case FixedTokenType::Uint8T:
			return IntegerType::uint8_t();
		case FixedTokenType::Uint16T:
			return IntegerType::uint16_t();
		case FixedTokenType::Uint32T:
			return IntegerType::uint32_t();
		case FixedTokenType::Uint64T:
			return IntegerType::uint64_t();
		case FixedTokenType::Int8T:
			return IntegerType::int8_t();
		case FixedTokenType::Int16T:
			return IntegerType::int16_t();
		case FixedTokenType::Int32T:
			return IntegerType::int32_t();
		case FixedTokenType::Int64T:
			return IntegerType::int64_t();
		case FixedTokenType::Float:
			return std::make_shared<FloatingPointType>(FloatingPointPrecision::Float);
		case FixedTokenType::Double:
			return std::make_shared<FloatingPointType>(FloatingPointPrecision::Double);
		case FixedTokenType::String:
			return std::make_shared<StringType>(CharacterWidth::Narrow);
		case FixedTokenType::WString:
			return std::make_shared<StringType>(CharacterWidth::Wide);
		default:
			throw GenericException("Internal error: program in unknown state!");
	}
}

std::shared_ptr<Type> UnaryTypeSpecificationNonTerminal::create_type() const{
	switch (this->outer_type) {
		case FixedTokenType::Pointer:
			return std::make_shared<PointerType>(this->inner_type->create_type());
		case FixedTokenType::SharedPtr:
			return std::make_shared<SharedPtrType>(this->inner_type->create_type());
		case FixedTokenType::UniquePtr:
			return std::make_shared<UniquePtrType>(this->inner_type->create_type());
		case FixedTokenType::Vector:
			return std::make_shared<VectorType>(this->inner_type->create_type());
		case FixedTokenType::List:
			return std::make_shared<ListType>(this->inner_type->create_type());
		case FixedTokenType::Set:
			return std::make_shared<SetType>(this->inner_type->create_type());
		case FixedTokenType::UnorderedSet:
			return std::make_shared<HashSetType>(this->inner_type->create_type());
		default:
			throw GenericException("Internal error: program in unknown state!");
	}
}

std::shared_ptr<Type> BinaryTypeSpecificationNonTerminal::create_type() const{
	auto type1 = this->inner_types[0]->create_type(),
		type2 = this->inner_types[1]->create_type();
	switch (this->outer_type){
		case FixedTokenType::Map:
			return std::make_shared<MapType>(type1, type2);
		case FixedTokenType::UnorderedMap:
			return std::make_shared<HashMapType>(type1, type2);
		default:
			throw GenericException("Internal error: program in unknown state!");
	}
}
