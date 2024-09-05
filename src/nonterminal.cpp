#include "nonterminal.hpp"
#include "errors.hpp"
#include <deque>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>

void require_token_type(std::deque<std::shared_ptr<Token>> &input, TokenType type){
	if (!input.size())
		throw ParsingError();
	if (input.front()->token_type() != type)
		throw ParsingError();
}

void require_fixed_token(std::deque<std::shared_ptr<Token>> &input, FixedTokenType type){
	if (input.empty() || *input.front() != type)
		throw ParsingError();
}

FileNonTerminal::FileNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	while (input.size()){
		auto &front = *input.front();
		switch (front.token_type()){
			case TokenType::FixedToken:
				switch (static_cast<const FixedToken &>(front).get_type()){
					case FixedTokenType::Cpp:
						this->declarations.push_back(std::make_shared<CppNonTerminal>(input));
						break;
					case FixedTokenType::Decl:
						this->declarations.push_back(std::make_shared<DeclNonTerminal>(input));
						break;
					default:
						throw ParsingError();
				}
				break;
			case TokenType::IdentifierToken:
				this->declarations.push_back(std::make_shared<VerbatimAssignmentNonTerminal>(input));
				break;
			default:
				throw ParsingError();
		}
	}
}

EnumMemberNonTerminal::EnumMemberNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::Equals);
	input.pop_front();

	require_token_type(input, TokenType::IntegerToken);
	this->value = std::static_pointer_cast<IntegerToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::Comma);
	input.pop_front();
}

std::shared_ptr<Type> create_basic_type(FixedTokenType t){
	switch (t){
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
		default:
			return {};
	}
}

EnumNonTerminal::EnumNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Enum);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::Colon);
	input.pop_front();

	require_token_type(input, TokenType::FixedToken);
	auto temp = std::static_pointer_cast<FixedToken>(input.front());
	input.pop_front();

	switch (this->underlying_type = temp->get_type()){
		case FixedTokenType::Uint8T:
		case FixedTokenType::Uint16T:
		case FixedTokenType::Uint32T:
		case FixedTokenType::Uint64T:
		case FixedTokenType::Int8T:
		case FixedTokenType::Int16T:
		case FixedTokenType::Int32T:
		case FixedTokenType::Int64T:
			break;
		default:
			throw ParsingError();
	}

	require_fixed_token(input, FixedTokenType::LBrace);
	input.pop_front();

	while (!input.empty() && *input.front() != FixedTokenType::RBrace)
		this->members.push_back(std::make_shared<EnumMemberNonTerminal>(input));

	require_fixed_token(input, FixedTokenType::RBrace);
	input.pop_front();
}

std::pair<std::shared_ptr<CppElement>, std::shared_ptr<Type>> EnumNonTerminal::generate_element_and_type(CppEvaluationState &state) const{
	auto t = std::make_shared<UserEnum>(state, this->get_name(), state.current_namespace, create_basic_type(this->underlying_type));
	return { t, t };
}

void EnumNonTerminal::finish_declaration(const std::shared_ptr<CppElement> &element, CppEvaluationState &state) const{
	auto Enum = std::dynamic_pointer_cast<UserEnum>(element);
	if (!Enum)
		throw std::runtime_error("Internal error: program in unknown state!");

	for (auto &m : this->members){
		auto [name, value] = m->get();
		Enum->set(std::move(name), std::move(value));
	}
}

IncludeDeclNonTerminal::IncludeDeclNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::IncludeDecl);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->declaration_to_include = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();
}

void IncludeDeclNonTerminal::update(CppEvaluationState &state){
	auto name = this->declaration_to_include->get_name();
	auto it = state.decls.find(name);
	if (it == state.decls.end())
		throw std::runtime_error("no such decl: " + name);
	for (auto &decl : it->second)
		decl->update(state);
}

TopLevelDeclarationNonTerminal::~TopLevelDeclarationNonTerminal(){}

CppNonTerminal::CppNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Cpp);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::LBrace);
	input.pop_front();

	while (!input.empty() && *input.front() != FixedTokenType::RBrace)
		this->declarations.push_back(CppDeclarationNonTerminal::create(input));

	require_fixed_token(input, FixedTokenType::RBrace);
	input.pop_front();
}

DeclNonTerminal::DeclNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Decl);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::LBrace);
	input.pop_front();

	while (input.size()){
		if (*input.front() == FixedTokenType::RBrace)
			break;
		this->declarations.push_back(TypeOrNamespaceNonTerminal::create(input));
	}
	require_fixed_token(input, FixedTokenType::RBrace);
	input.pop_front();
}

std::shared_ptr<TypeDeclarationNonTerminal> create_class(std::deque<std::shared_ptr<Token>> &input, bool assume_abstract){
	auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
	switch (type){
		case FixedTokenType::Abstract:
			if (assume_abstract)
				throw ParsingError();
			input.pop_front();
			return create_class(input, true);
		case FixedTokenType::Struct:
			return std::make_shared<ClassNonTerminal>(input, true, assume_abstract);
		case FixedTokenType::Class:
			return std::make_shared<ClassNonTerminal>(input, false, assume_abstract);
		default:
			throw ParsingError();
	}
}

std::shared_ptr<TypeDeclarationNonTerminal> TypeDeclarationNonTerminal::create(std::deque<std::shared_ptr<Token>> &input, bool assume_abstract){
	require_token_type(input, TokenType::FixedToken);
	auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
	switch (type){
		case FixedTokenType::Abstract:
		case FixedTokenType::Struct:
		case FixedTokenType::Class:
			return create_class(input, assume_abstract);
		case FixedTokenType::Enum:
			return std::make_shared<EnumNonTerminal>(input);
		default:
			throw ParsingError();
	}
}

std::shared_ptr<CppDeclarationNonTerminal> CppDeclarationNonTerminal::create(std::deque<std::shared_ptr<Token>> &input){
	if (input.empty())
		throw ParsingError();
	switch (input.front()->token_type()){
		case TokenType::FixedToken:
			switch (std::static_pointer_cast<FixedToken>(input.front())->get_type()){
				case FixedTokenType::IncludeDecl:
					return std::make_shared<IncludeDeclNonTerminal>(input);
				case FixedTokenType::LocalInclude:
				case FixedTokenType::GlobalInclude:
					return std::make_shared<IncludeDirectiveNonTerminal>(input);
				case FixedTokenType::Namespace:
					return std::make_shared<CppNamespaceDeclNonTerminal>(input);
				default:
					break;
			}
			return TypeDeclarationNonTerminal::create(input);
		case TokenType::IdentifierToken:
			return std::make_shared<VerbatimAssignmentNonTerminal>(input);
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

ClassInternalNonTerminal::~ClassInternalNonTerminal(){}

std::shared_ptr<ClassInternalNonTerminal> ClassInternalNonTerminal::create(std::deque<std::shared_ptr<Token>> &input){
	if (!input.size())
		throw ParsingError();
	switch (input.front()->token_type()){
		case TokenType::FixedToken:
			break;
		case TokenType::IdentifierToken:
			return std::make_shared<DataDeclarationNonTerminal>(input);
		case TokenType::VerbatimBlockToken:
			return std::make_shared<VerbatimBlockNonTerminal>(input);
		default:
			throw ParsingError();
	}
	auto type = std::static_pointer_cast<FixedToken>(input.front())->get_type();
	switch (type){
		case FixedTokenType::CustomDtor:
			return std::make_shared<CustomDtorNonTerminal>(input);
		case FixedTokenType::Dollar:
			return std::make_shared<NamedVerbatimUseNonTerminal>(input);
		default:
			break;
	}
	if (is_typename_token(type))
		return std::make_shared<DataDeclarationNonTerminal>(input);
	if (is_access_specifier_token(type))
		return std::make_shared<AccessSpecifierNonTerminal>(input);
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
	if (*input.front() == FixedTokenType::LocalInclude){
		this->relative = true;
	}else if (*input.front() == FixedTokenType::GlobalInclude){
		this->relative = false;
	}else
		throw ParsingError();
	input.pop_front();

	require_token_type(input, TokenType::StringLiteralToken);
	this->path = std::static_pointer_cast<StringLiteralToken>(input.front());
	input.pop_front();
}

std::shared_ptr<CppElement> IncludeDirectiveNonTerminal::generate_element(CppFile &file) const{
	return std::make_shared<UserInclude>(file, this->path->get_value(), this->relative);
}

DataDeclarationNonTerminal::DataDeclarationNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	if (!input.size())
		throw ParsingError();
	if (input.front()->token_type() == TokenType::FixedToken){
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
	if (type == FixedTokenType::LBracket){
		input.pop_front();

		require_token_type(input, TokenType::IntegerToken);
		this->length = std::static_pointer_cast<IntegerToken>(input.front());
		input.pop_front();

		require_fixed_token(input, FixedTokenType::RBracket);
		input.pop_front();
	}

	require_fixed_token(input, FixedTokenType::Semicolon);
	input.pop_front();
}

CppEvaluationState::CppEvaluationState(EvaluationResult &er): decls(er.get_decls()){
	this->verbatim_declarations = er.get_verbatims();
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
		case FixedTokenType::U32String:
			input.pop_front();
			return std::make_shared<NullaryTypeSpecificationNonTerminal>(type);
		case FixedTokenType::Pointer:
		case FixedTokenType::SharedPtr:
		case FixedTokenType::UniquePtr:
		case FixedTokenType::Vector:
		case FixedTokenType::List:
		case FixedTokenType::Set:
		case FixedTokenType::UnorderedSet:
		case FixedTokenType::Optional:
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

void CppNonTerminal::update(EvaluationResult &result) const{
	result.add(this->evaluate_ast(result));
}

void DeclNonTerminal::forward_declare(EvaluationResult &result) const{
	result.add(this->name->get_name(), this->declarations);
}

VerbatimAssignmentNonTerminal::VerbatimAssignmentNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::Equals);
	input.pop_front();

	require_token_type(input, TokenType::VerbatimBlockToken);
	this->content = std::static_pointer_cast<VerbatimBlockToken>(input.front());
	input.pop_front();
}

void VerbatimAssignmentNonTerminal::forward_declare(EvaluationResult &evaluation_result) const{
	evaluation_result.add_verbatim(this->name->get_name(), this->content->get_content());
}

void VerbatimAssignmentNonTerminal::forward_declare(CppEvaluationState &state){
	state.add_verbatim(this->name->get_name(), this->content->get_content());
}

EvaluationResult FileNonTerminal::evaluate_ast() const{
	EvaluationResult ret;
	for (auto &decl : this->declarations)
		decl->forward_declare(ret);
	for (auto &decl : this->declarations)
		decl->update(ret);
	return ret;
}

std::shared_ptr<CppFile> CppNonTerminal::evaluate_ast(EvaluationResult &er) const{
	CppEvaluationState state(er);
	state.result = std::make_shared<CppFile>(this->name->get_name());

	for (auto &declaration : this->declarations)
		declaration->forward_declare(state);

	for (auto &declaration : this->declarations)
		declaration->update(state);

	for (auto &[type_decl, element] : state.declarations){
		if (type_decl)
			type_decl->finish_declaration(element, state);
		state.result->add_element(element);
	}

	state.result->mark_virtual_inheritances();

	return state.result;
}

void IncludeDirectiveNonTerminal::update(CppEvaluationState &state){
	auto element = this->generate_element(*state.result);
	state.declarations.emplace_back(std::shared_ptr<TypeDeclarationNonTerminal>{}, std::move(element));
}

std::shared_ptr<TypeOrNamespaceOrIncludeDeclNonTerminal> TypeOrNamespaceOrIncludeDeclNonTerminal::create(std::deque<std::shared_ptr<Token>> &input){
	if (input.empty())
		throw ParsingError();
	if (*input.front() == FixedTokenType::IncludeDecl)
		return std::make_shared<IncludeDeclNonTerminal>(input);
	return TypeOrNamespaceNonTerminal::create(input);
}

std::shared_ptr<TypeOrNamespaceNonTerminal> TypeOrNamespaceNonTerminal::create(std::deque<std::shared_ptr<Token>> &input){
	if (input.empty())
		throw ParsingError();
	if (*input.front() == FixedTokenType::Namespace)
		return std::make_shared<NamespaceDeclNonTerminal>(input);
	return TypeDeclarationNonTerminal::create(input);
}

void TypeDeclarationNonTerminal::update(CppEvaluationState &state){
	auto [element, type] = this->generate_element_and_type(state);
	state.dsl_type_map[type->get_serializer_name()] = std::move(type);
	state.declarations.emplace_back(this->shared_from_this(), std::move(element));
}

void BasicNamespaceDeclNonTerminal::pre(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Namespace);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();

	require_fixed_token(input, FixedTokenType::LBrace);
	input.pop_front();
}

void BasicNamespaceDeclNonTerminal::post(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::RBrace);
	input.pop_front();
}

CppNamespaceDeclNonTerminal::CppNamespaceDeclNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	this->pre(input);
	while (!input.empty() && *input.front() != FixedTokenType::RBrace)
		this->declarations.push_back(TypeOrNamespaceOrIncludeDeclNonTerminal::create(input));
	this->post(input);
}

NamespaceDeclNonTerminal::NamespaceDeclNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	this->pre(input);
	while (!input.empty() && *input.front() != FixedTokenType::RBrace)
		this->declarations.push_back(TypeOrNamespaceOrIncludeDeclNonTerminal::create(input));
	this->post(input);
}

void CppNamespaceDeclNonTerminal::update(CppEvaluationState &state){
	state.current_namespace.push_back(this->name->get_name());
	for (auto &decl : this->declarations)
		decl->update(state);
	state.current_namespace.pop_back();
}

void NamespaceDeclNonTerminal::update(CppEvaluationState &state){
	state.current_namespace.push_back(this->name->get_name());
	for (auto &decl : this->declarations)
		decl->update(state);
	state.current_namespace.pop_back();
}

std::pair<std::shared_ptr<CppElement>, std::shared_ptr<Type>> ClassNonTerminal::generate_element_and_type(CppEvaluationState &state) const{
	auto t = std::make_shared<UserClass>(*state.result, this->name->get_name(), state.current_namespace, this->abstract);
	return { t, t };
}

void CppNonTerminal::insert(std::unordered_set<std::string> &types, const DeclNonTerminal &include){
	for (auto &thing : include){
		auto type = std::dynamic_pointer_cast<TypeDeclarationNonTerminal>(thing);
		if (!type)
			continue;
		auto name = type->get_name();
		auto it = types.find(name);
		if (it != types.end())
			throw std::runtime_error("duplicate type declaration " + name + " in cpp " + this->name->get_name());
		this->declarations.push_back(type);
		types.insert(name);
	}
}

void ClassNonTerminal::finish_declaration(const std::shared_ptr<CppElement> &element, CppEvaluationState &state) const{
	auto Class = std::dynamic_pointer_cast<UserClass>(element);

	if (!Class)
		throw std::runtime_error("Internal error: program in unknown state!");

	for (auto &b : this->base_classes){
		auto it = state.dsl_type_map.find(b->get_name());
		if (it == state.dsl_type_map.end())
			continue;
		auto type = std::dynamic_pointer_cast<UserClass>(it->second);
		Class->add_base_class(type);
	}

	auto current_accessibility = this->public_default ? Accessibility::Public : Accessibility::Private;
	for (auto &i : this->internals){
		i->modify_class(Class, current_accessibility, state);
	}
}

void AccessSpecifierNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &state) const{
	switch (this->access){
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
			throw std::exception();
	}
}

void DataDeclarationNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &state) const{
	auto type = this->type->create_type(state.dsl_type_map);
	if (this->length)
		type = std::make_shared<ArrayType>(type, this->length->get_value());
	auto name = this->name->get_name();
	auto member = std::make_shared<ClassMember>(type, name, current_accessibility);
	Class->add_element(member);
}

void VerbatimBlockNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &state) const{
	Class->add_element(std::make_shared<VerbatimBlock>(this->content->get_content()));
}

void CustomDtorNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &state) const{
	Class->no_default_destructor();
}

void NamedVerbatimUseNonTerminal::modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &state) const{
	auto &verbatims = state.get_verbatims();
	auto name = this->name->get_name();
	auto it = verbatims.find(name);
	if (it == verbatims.end())
		throw std::runtime_error("verbatim block " + name + " not defined");
	Class->add_element(std::make_shared<VerbatimBlock>(it->second));
}

NamedVerbatimUseNonTerminal::NamedVerbatimUseNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::Dollar);
	input.pop_front();

	require_token_type(input, TokenType::IdentifierToken);
	this->name = std::static_pointer_cast<IdentifierToken>(input.front());
	input.pop_front();
}

CustomDtorNonTerminal::CustomDtorNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_fixed_token(input, FixedTokenType::CustomDtor);
	input.pop_front();
}

VerbatimBlockNonTerminal::VerbatimBlockNonTerminal(std::deque<std::shared_ptr<Token>> &input){
	require_token_type(input, TokenType::VerbatimBlockToken);
	this->content = std::static_pointer_cast<VerbatimBlockToken>(input.front());
	input.pop_front();
}

std::shared_ptr<Type> UserTypeSpecificationNonTerminal::create_type(dsl_type_map_t &dsl_type_map) const{
	auto name = this->name->get_name();
	auto it = dsl_type_map.find(name);
	if (it == dsl_type_map.end())
		throw ParsingError();

	return it->second;
}

std::shared_ptr<Type> NullaryTypeSpecificationNonTerminal::create_type(dsl_type_map_t &dsl_type_map) const{
	if (auto ret = create_basic_type(this->type))
		return ret;
	switch (this->type){
		case FixedTokenType::String:
			return std::make_shared<StringType>(CharacterWidth::Narrow);
		case FixedTokenType::U32String:
			return std::make_shared<StringType>(CharacterWidth::Wide);
		default:
			throw std::runtime_error("Internal error: program in unknown state!");
	}
}

std::shared_ptr<Type> UnaryTypeSpecificationNonTerminal::create_type(dsl_type_map_t &dsl_type_map) const{
	switch (this->outer_type){
		case FixedTokenType::Pointer:
			return std::make_shared<PointerType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::SharedPtr:
			return std::make_shared<SharedPtrType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::UniquePtr:
			return std::make_shared<UniquePtrType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::Vector:
			return std::make_shared<VectorType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::List:
			return std::make_shared<ListType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::Set:
			return std::make_shared<SetType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::UnorderedSet:
			return std::make_shared<HashSetType>(this->inner_type->create_type(dsl_type_map));
		case FixedTokenType::Optional:
			return std::make_shared<OptionalType>(this->inner_type->create_type(dsl_type_map));
		default:
			throw std::runtime_error("Internal error: program in unknown state!");
	}
}

std::shared_ptr<Type> BinaryTypeSpecificationNonTerminal::create_type(dsl_type_map_t &dsl_type_map) const{
	auto type1 = this->inner_types[0]->create_type(dsl_type_map),
		type2 = this->inner_types[1]->create_type(dsl_type_map);
	switch (this->outer_type){
		case FixedTokenType::Map:
			return std::make_shared<MapType>(type1, type2);
		case FixedTokenType::UnorderedMap:
			return std::make_shared<HashMapType>(type1, type2);
		default:
			throw std::runtime_error("Internal error: program in unknown state!");
	}
}
