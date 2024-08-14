#include "token.h"
#include <cassert>
#include <cctype>
#include <stdexcept>

#include "errors.h"

using namespace std::string_literals;

const char * const whitespace = " \f\t\r\n";
const char * const simple_tokens = "<>{}[]:;#,$=";

const char *const keywords[] = {
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
	"",
	"bool",
	"u8",
	"u16",
	"u32",
	"u64",
	"i8",
	"i16",
	"i32",
	"i64",
	"float",
	"double",
	"unordered_set",
	"unordered_map",
	"string",
	"u32string",
	"abstract",
	"optional",
	"decl",
	"include_decl",
	"local_include",
	"global_include",
	"custom_dtor",
	"namespace",
	"enum",
	nullptr,
};

bool is_first_identifier_char(char c){
	c = (char)tolower(c);
	return c == '_' || c >= 'a' && c <= 'z';
}

bool is_nth_identifier_char(char c){
	return is_first_identifier_char(c) || c >= '0' && c <= '9';
}

template <typename T>
bool char_matches_any(T c, const T *p){
	for (; *p; p++)
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

std::string read_identifier(std::deque<char> &input){
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
	return identifier;
}

bool Token::operator==(FixedTokenType t) const{
	if (this->token_type() != TokenType::FixedToken)
		return false;
	auto &This = static_cast<const FixedToken &>(*this);
	return This.get_type() == t;
}

bool terminator_next(std::deque<char> &input){
	static const char terminator[] = "}verbatim";
	static const size_t terminator_length = sizeof(terminator) - 1;
	if (input.size() < terminator_length)
		return false;
	for (size_t i = 0; i < terminator_length; i++)
		if (input[i] != terminator[i])
			return false;
	for (size_t i = 0; i < terminator_length; i++)
		input.pop_front();
	return true;
}

std::string read_verbatim_block(std::deque<char> &input){
	std::string ret;
	while (!input.empty()){
		if (terminator_next(input))
			return ret;
		ret += input.front();
		input.pop_front();
	}
	throw ParsingError();
}

std::shared_ptr<FixedToken> match_keyword(const std::string &id){
	for (size_t i = 0; keywords[i]; i++)
		if (id == keywords[i])
			return std::make_shared<FixedToken>((FixedTokenType)(first_name_token + i));
	return {};
}

std::deque<std::shared_ptr<Token>> tokenize(std::deque<char> &input){
	std::deque<std::shared_ptr<Token>> ret;
	bool seen_first_comment_char = false;
	bool in_comment = false;
	bool seen_equals = false;
	while (input.size()){
		auto c = input.front();
		if (seen_equals){
			if (c != '>')
				throw std::runtime_error("expected '>' after '=' bu found '"s + c + "'");
			ret.push_back(std::make_shared<FixedToken>(FixedTokenType::DoubleRightArrow));
			seen_equals = false;
			input.pop_front();
			continue;
		}
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
			throw std::exception();
		}
		if (c == '/'){
			seen_first_comment_char = true;
			input.pop_front();
			continue;
		}
		if (is_whitespace(c)){
			input.pop_front();
			continue;
		}
		if (is_simple_token(c)){
			ret.push_back(std::make_shared<FixedToken>((FixedTokenType)c));
			input.pop_front();
			continue;
		}
		if (c == '='){
			seen_equals = true;
			input.pop_front();
			continue;
		}
		if (is_first_identifier_char(c)){
			auto identifier = read_identifier(input);
			if (identifier == "verbatim"){
				if (input.empty() || input.front() != '{')
					throw ParsingError();
				input.pop_front();
				ret.push_back(std::make_shared<VerbatimBlockToken>(read_verbatim_block(input)));
				continue;
			}
			std::shared_ptr<Token> token = match_keyword(identifier);
			if (!token)
				token = std::make_shared<IdentifierToken>(identifier);
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
		if (c == '-' || isdigit(c)){
			bool flip_sign = false;
			if (c == '-'){
				flip_sign = true;
				input.pop_front();
			}
			EasySignedBigNum integer;
			while (!input.empty()){
				auto c = input.front();
				if (!isdigit(c))
					break;
				integer *= 10;
				integer += c - '0';
				input.pop_front();
			}
			if (flip_sign)
				integer = -integer;
			ret.push_back(std::make_shared<IntegerToken>(std::move(integer)));
			continue;
		}
		throw std::exception();
	}
	return ret;
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
		case FixedTokenType::U32String:
		case FixedTokenType::Optional:
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
