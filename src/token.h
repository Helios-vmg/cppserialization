#pragma once

#include <string>
#include <memory>
#include <queue>

const int first_name_token = 1000;
const int first_long_token = 256;

enum class TokenType{
	FixedToken,
	IdentifierToken,
	IntegerToken,
	StringLiteralToken,
	VerbatimBlockToken,
};

enum class FixedTokenType{
	LParen        = '(',
	RParen        = ')',
	LBrace        = '{',
	RBrace        = '}',
	LAngleBracket = '<',
	RAngleBracket = '>',
	LBracket      = '[',
	RBracket      = ']',
	Colon         = ':',
	Semicolon     = ';',
	Comma         = ',',
	Dollar        = '$',
	Equals        = '=',

	DoubleRightArrow   = first_long_token + 0,

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
	U32String     = first_name_token + 28,
	Abstract      = first_name_token + 29,
	Optional      = first_name_token + 30,
	Decl          = first_name_token + 31,
	IncludeDecl   = first_name_token + 32,
	LocalInclude  = first_name_token + 33,
	GlobalInclude = first_name_token + 34,
	CustomDtor    = first_name_token + 35,
	Namespace     = first_name_token + 36,
};

enum class AccessType{
	Public,
	Protected,
	Private,
};

class Token{
public:
	virtual ~Token(){}
	virtual TokenType token_type() const = 0;
	bool operator==(FixedTokenType t) const;
	bool operator!=(FixedTokenType t) const{
		return !(*this == t);
	}
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

class VerbatimBlockToken : public Token{
	std::string content;
public:
	VerbatimBlockToken(const std::string &value): content(value){}
	const std::string &get_content() const{
		return this->content;
	}
	TokenType token_type() const override{
		return TokenType::VerbatimBlockToken;
	}
};

std::deque<std::shared_ptr<Token>> tokenize(std::deque<char> &input);
bool is_typename_token(FixedTokenType type);
bool is_access_specifier_token(FixedTokenType type);
