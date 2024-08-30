#pragma once

#include <unordered_set>

#include "las.h"
#include "nonterminal.h"
#include "token.h"

typedef std::unordered_map<std::string, std::shared_ptr<Type>> dsl_type_map_t;

class TypeDeclarationNonTerminal;
class EvaluationResult;
class TypeOrNamespaceNonTerminal;

class VerbatimContainer{
protected:
	std::map<std::string, std::string> verbatim_declarations;
public:
	void add_verbatim(std::string, std::string);
	auto &get_verbatims() const{
		return this->verbatim_declarations;
	}
};

class CppEvaluationState : public VerbatimContainer{
public:
	const std::map<std::string, std::vector<std::shared_ptr<TypeOrNamespaceNonTerminal>>> &decls;
	dsl_type_map_t dsl_type_map;
	std::vector<std::pair<std::shared_ptr<TypeDeclarationNonTerminal>, std::shared_ptr<CppElement>>> declarations;
	std::shared_ptr<CppFile> result;
	std::vector<std::string> current_namespace;
	std::uint32_t next_enum_id = 1;

	CppEvaluationState(EvaluationResult &);
};

class TypeSpecificationNonTerminal{
public:
	virtual ~TypeSpecificationNonTerminal() = 0;

	virtual std::shared_ptr<Type> create_type(dsl_type_map_t &dsl_type_map) const = 0;
	static std::shared_ptr<TypeSpecificationNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
};

class UserTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	std::shared_ptr<IdentifierToken> name;
public:
	UserTypeSpecificationNonTerminal(const std::shared_ptr<IdentifierToken> &name) : name(name){}
	std::shared_ptr<Type> create_type(dsl_type_map_t &dsl_type_map) const override;
};

class NullaryTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	FixedTokenType type;
public:
	NullaryTypeSpecificationNonTerminal(const FixedTokenType &type) : type(type){}
	std::shared_ptr<Type> create_type(dsl_type_map_t &dsl_type_map) const override;
};

class UnaryTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	FixedTokenType outer_type;
	std::shared_ptr<TypeSpecificationNonTerminal> inner_type;
public:
	UnaryTypeSpecificationNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::shared_ptr<Type> create_type(dsl_type_map_t &dsl_type_map) const override;
};

class BinaryTypeSpecificationNonTerminal : public TypeSpecificationNonTerminal{
	FixedTokenType outer_type;
	std::shared_ptr<TypeSpecificationNonTerminal> inner_types[2];
public:
	BinaryTypeSpecificationNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::shared_ptr<Type> create_type(dsl_type_map_t &dsl_type_map) const override;
};

class ClassInternalNonTerminal{
public:
	virtual ~ClassInternalNonTerminal() = 0;
	static std::shared_ptr<ClassInternalNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
	virtual void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &) const = 0;
};

class AccessSpecifierNonTerminal : public ClassInternalNonTerminal{
	AccessType access;
public:
	AccessSpecifierNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &) const override;
};

class DataDeclarationNonTerminal : public ClassInternalNonTerminal{
	std::shared_ptr<TypeSpecificationNonTerminal> type;
	std::shared_ptr<IdentifierToken> name;
	std::shared_ptr<IntegerToken> length;
public:
	DataDeclarationNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &) const override;
};

class VerbatimBlockNonTerminal : public ClassInternalNonTerminal{
	std::shared_ptr<VerbatimBlockToken> content;
public:
	VerbatimBlockNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &) const override;
};

class CustomDtorNonTerminal : public ClassInternalNonTerminal{
public:
	CustomDtorNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &) const override;
};

class NamedVerbatimUseNonTerminal : public ClassInternalNonTerminal{
	std::shared_ptr<IdentifierToken> name;
public:
	NamedVerbatimUseNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void modify_class(const std::shared_ptr<UserClass> &Class, Accessibility &current_accessibility, CppEvaluationState &) const override;
};

class CppDeclarationNonTerminal{
public:
	virtual ~CppDeclarationNonTerminal(){}
	static std::shared_ptr<CppDeclarationNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
	virtual void forward_declare(CppEvaluationState &) = 0;
	virtual void update(CppEvaluationState &) = 0;
};

class IncludeDirectiveNonTerminal : public CppDeclarationNonTerminal{
	std::shared_ptr<StringLiteralToken> path;
	bool relative;

	std::shared_ptr<CppElement> generate_element(CppFile &file) const;
public:
	IncludeDirectiveNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void forward_declare(CppEvaluationState &) override{}
	void update(CppEvaluationState &) override;
};

class TypeOrNamespaceOrIncludeDeclNonTerminal : public CppDeclarationNonTerminal{
public:
	TypeOrNamespaceOrIncludeDeclNonTerminal() = default;
	virtual ~TypeOrNamespaceOrIncludeDeclNonTerminal(){}
	static std::shared_ptr<TypeOrNamespaceOrIncludeDeclNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
};

class TypeOrNamespaceNonTerminal : public TypeOrNamespaceOrIncludeDeclNonTerminal{
public:
	TypeOrNamespaceNonTerminal() = default;
	virtual ~TypeOrNamespaceNonTerminal(){}
	static std::shared_ptr<TypeOrNamespaceNonTerminal> create(std::deque<std::shared_ptr<Token>> &input);
};

class TypeDeclarationNonTerminal : public TypeOrNamespaceNonTerminal, public std::enable_shared_from_this<TypeDeclarationNonTerminal>{
protected:
	virtual std::pair<
		std::shared_ptr<CppElement>,
		std::shared_ptr<Type>
	> generate_element_and_type(CppEvaluationState &) const = 0;

public:
	virtual ~TypeDeclarationNonTerminal(){}
	static std::shared_ptr<TypeDeclarationNonTerminal> create(std::deque<std::shared_ptr<Token>> &input, bool assume_abstract = false);
	virtual std::string get_name() const = 0;
	void update(CppEvaluationState &) override;
	virtual void finish_declaration(const std::shared_ptr<CppElement> &, CppEvaluationState &) const = 0;
};

class BasicNamespaceDeclNonTerminal{
protected:
	std::shared_ptr<IdentifierToken> name;

	void pre(std::deque<std::shared_ptr<Token>> &input);
	void post(std::deque<std::shared_ptr<Token>> &input);
public:
	BasicNamespaceDeclNonTerminal() = default;
	virtual ~BasicNamespaceDeclNonTerminal(){}
};

class CppNamespaceDeclNonTerminal : public BasicNamespaceDeclNonTerminal, public TypeOrNamespaceNonTerminal{
	std::vector<std::shared_ptr<TypeOrNamespaceOrIncludeDeclNonTerminal>> declarations;
public:
	CppNamespaceDeclNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void forward_declare(CppEvaluationState &) override{}
	void update(CppEvaluationState &) override;
};

class NamespaceDeclNonTerminal : public BasicNamespaceDeclNonTerminal, public TypeOrNamespaceNonTerminal{
	std::vector<std::shared_ptr<TypeOrNamespaceOrIncludeDeclNonTerminal>> declarations;
public:
	NamespaceDeclNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void forward_declare(CppEvaluationState &) override{}
	void update(CppEvaluationState &) override;
};

class ClassNonTerminal : public TypeDeclarationNonTerminal{
protected:
	bool public_default;
	bool abstract;
	std::shared_ptr<IdentifierToken> name;
	std::vector<std::shared_ptr<IdentifierToken>> base_classes;
	std::vector<std::shared_ptr<ClassInternalNonTerminal>> internals;

	std::pair<
		std::shared_ptr<CppElement>,
		std::shared_ptr<Type>
	> generate_element_and_type(CppEvaluationState &) const override;
public:
	ClassNonTerminal(std::deque<std::shared_ptr<Token>> &input, bool public_default, bool is_abstract = false);
	void forward_declare(CppEvaluationState &) override{}
	void finish_declaration(const std::shared_ptr<CppElement> &, CppEvaluationState &) const override;
	std::string get_name() const override{
		return this->name->get_name();
	}
};

class EnumMemberNonTerminal{
	std::shared_ptr<IdentifierToken> name;
	std::shared_ptr<IntegerToken> value;
public:
	EnumMemberNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::pair<std::string, EasySignedBigNum> get() const{
		return { this->name->get_name(), this->value->get_value() };
	}
};

class EnumNonTerminal : public TypeDeclarationNonTerminal{
	std::shared_ptr<IdentifierToken> name;
	FixedTokenType underlying_type;
	std::vector<std::shared_ptr<EnumMemberNonTerminal>> members;

	std::pair<std::shared_ptr<CppElement>, std::shared_ptr<Type>> generate_element_and_type(CppEvaluationState &) const override;
public:
	EnumNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void forward_declare(CppEvaluationState &) override{}
	std::string get_name() const override{
		return this->name->get_name();
	}
	void finish_declaration(const std::shared_ptr<CppElement> &, CppEvaluationState &) const override;
};

class IncludeDeclNonTerminal : public TypeOrNamespaceOrIncludeDeclNonTerminal{
	std::shared_ptr<IdentifierToken> declaration_to_include;
public:
	IncludeDeclNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	std::string get_target_name() const{
		return this->declaration_to_include->get_name();
	}
	void forward_declare(CppEvaluationState &) override{}
	void update(CppEvaluationState &) override;
};

class DeclNonTerminal;

class TopLevelDeclarationNonTerminal{
public:
	virtual ~TopLevelDeclarationNonTerminal() = 0;
	virtual void forward_declare(EvaluationResult &) const{}
	virtual void update(EvaluationResult &) const{}
};

class CppNonTerminal : public TopLevelDeclarationNonTerminal{
	std::shared_ptr<IdentifierToken> name;
	std::vector<std::shared_ptr<CppDeclarationNonTerminal>> declarations;

	void insert(std::unordered_set<std::string> &, const DeclNonTerminal &);
	std::shared_ptr<CppFile> evaluate_ast(EvaluationResult &) const;
public:
	CppNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void update(EvaluationResult &) const override;
};

class DeclNonTerminal : public TopLevelDeclarationNonTerminal{
	std::shared_ptr<IdentifierToken> name;
	std::vector<std::shared_ptr<TypeOrNamespaceNonTerminal>> declarations;
public:
	DeclNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void forward_declare(EvaluationResult &) const override;
	std::string get_name() const{
		return this->name->get_name();
	}
	auto begin() const{
		return this->declarations.begin();
	}
	auto end() const{
		return this->declarations.end();
	}
};

class VerbatimAssignmentNonTerminal : public CppDeclarationNonTerminal, public TopLevelDeclarationNonTerminal{
	std::shared_ptr<IdentifierToken> name;
	std::shared_ptr<VerbatimBlockToken> content;
public:
	VerbatimAssignmentNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	void forward_declare(EvaluationResult &) const override;
	void forward_declare(CppEvaluationState &) override;
	void update(CppEvaluationState &) override{}
};

class EvaluationResult : public VerbatimContainer{
	std::map<std::string, std::vector<std::shared_ptr<TypeOrNamespaceNonTerminal>>> decls;
	std::map<std::string, std::string> verbatim_declarations;
	std::vector<std::shared_ptr<CppFile>> cpps;
public:
	EvaluationResult() = default;
	EvaluationResult(EvaluationResult &&) = default;
	EvaluationResult &operator=(EvaluationResult &&) = default;
	void add(std::shared_ptr<CppFile>);
	void add(std::string, std::vector<std::shared_ptr<TypeOrNamespaceNonTerminal>>);
	void generate();
	auto &get_decls() const{
		return this->decls;
	}
};

class FileNonTerminal{
	std::vector<std::shared_ptr<TopLevelDeclarationNonTerminal>> declarations;
public:
	FileNonTerminal(std::deque<std::shared_ptr<Token>> &input);
	EvaluationResult evaluate_ast() const;
};
