#pragma once

#include "typehash.hpp"
#include "EasyBigNum.hpp"
#include "variable_formatter.hpp"
#include <iostream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <functional>
#include <cstdint>
#include <random>
#include <optional>

#define SERIALIZER_VERSION 1

class CppEvaluationState;
extern std::mt19937_64 rng;
class CppFile;

class CppElement{
protected:
	CppFile *root;
public:
	CppElement(CppFile &file): root(&file){}
	virtual ~CppElement(){}
	CppFile &get_root() const{
		return *this->root;
	}
	virtual std::string generate_inclusion() const{
		return {};
	}
	virtual std::string generate_forward_declaration() const{
		return {};
	}
	virtual std::string generate_header() const = 0;
};

enum class CallMode{
	TransformAndAdd,
	TransformAndReturn,
	AddVerbatim,
};
typedef const std::function<std::string (const std::string &, CallMode)> generate_pointer_enumerator_callback_t;

enum class CppVersion{
	Undefined = 0,
	Cpp1998 = 199711,
	Cpp2011 = 201103,
	Cpp2014 = 201402,
	Cpp2017 = 201703,
	Cpp2020 = 202002,
};

class Type : public std::enable_shared_from_this<Type>{
	std::uint32_t type_id;
protected:
	TypeHash type_hash;
	typedef const std::function<void(Type &, std::uint32_t &)> iterate_callback_t;
	virtual void iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited){
		callback(*this, this->type_id);
	}
	virtual void iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore){
		if (do_not_ignore)
			callback(*this, this->type_id);
	}
public:
	virtual ~Type(){}
	virtual std::string get_source_name() const = 0;
	virtual std::string get_source_name(const std::string &name) const{
		return this->get_source_name() + " " + name;
	}
	virtual const char *header() const{
		return nullptr;
	}
	virtual void add_headers(std::set<std::string> &set){
		auto header = this->header();
		if (header)
			set.insert((std::string)header);
	}
	virtual void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{}
	virtual void generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const;
	virtual std::string generate_deserializer(const char *deserializer_name, const char *pointer_name) const{
		std::stringstream stream;
		this->generate_deserializer(stream, deserializer_name, pointer_name);
		return stream.str();
	}
	virtual void generate_rollbacker(std::ostream &stream, const char *pointer_name) const;
	void generate_is_serializable(std::ostream &stream) const;
	virtual bool get_is_serializable() const{
		return false;
	}
	std::uint32_t get_type_id() const{
		return this->type_id;
	}
	virtual std::string get_serializer_name() const = 0;
	virtual std::string base_get_type_string() const{
		return this->get_serializer_name();
	}
	virtual const TypeHash &get_type_hash(){
		if (!this->type_hash){
			std::stringstream stream;
			stream << "version" << SERIALIZER_VERSION << this->get_serializer_name();
			this->type_hash = TypeHash(stream.str());
		}
		return this->type_hash;
	}
	const TypeHash &get_type_hash() const{
		if (!this->type_hash)
			abort();
		return this->type_hash;
	}
	void iterate(iterate_callback_t &callback){
		std::set<Type *> visited;
		this->iterate(callback, visited);
	}
	void iterate(iterate_callback_t &callback, std::set<Type *> &visited){
		auto it = visited.find(this);
		if (it != visited.end())
			return;
		visited.insert(this);
		this->iterate_internal(callback, visited);
	}
	void iterate_only_public(iterate_callback_t &callback){
		std::set<Type *> visited;
		this->iterate_only_public(callback, visited, true);
	}
	void iterate_only_public(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore){
		auto it = visited.find(this);
		if (it != visited.end())
			return;
		visited.insert(this);
		this->iterate_only_public_internal(callback, visited, do_not_ignore);
	}
	virtual bool is_pointer_type() const{
		return false;
	}
	virtual bool is_abstract() const{
		return false;
	}
	virtual bool is_serializable() const{
		return false;
	}
	virtual CppVersion minimum_cpp_version() const{
		return CppVersion::Cpp2011;
	}
	virtual std::string fully_qualified_name() const{
		return this->get_source_name();
	}
	virtual bool check_value(const EasySignedBigNum &value){
		return false;
	}
	virtual std::shared_ptr<Type> get_underlying_type(){
		return this->shared_from_this();
	}
};

class BoolType : public Type{
public:
	using Type::get_source_name;
	std::string get_source_name() const override{
		return "bool";
	}
	std::string get_serializer_name() const override{
		return "b";
	}
};

class IntegerType : public Type{
	bool signedness;
	//valid sizes = 0, 1, 2, 3
	unsigned size;

	static std::shared_ptr<IntegerType> creator_helper(unsigned i){
		return std::make_shared<IntegerType>(!!(i & 1), i / 2);
	}
public:
	IntegerType(bool signedness, unsigned size):
		signedness(signedness),
		size(size){}
	static std::shared_ptr<IntegerType> uint8_t(){
		return creator_helper(0);
	}
	static std::shared_ptr<IntegerType> int8_t(){
		return creator_helper(1);
	}
	static std::shared_ptr<IntegerType> uint16_t(){
		return creator_helper(2);
	}
	static std::shared_ptr<IntegerType> int16_t(){
		return creator_helper(3);
	}
	static std::shared_ptr<IntegerType> uint32_t(){
		return creator_helper(4);
	}
	static std::shared_ptr<IntegerType> int32_t(){
		return creator_helper(5);
	}
	static std::shared_ptr<IntegerType> uint64_t(){
		return creator_helper(6);
	}
	static std::shared_ptr<IntegerType> int64_t(){
		return creator_helper(7);
	}
	using Type::get_source_name;
	std::string get_source_name() const override;
	const char *header() const override{
		return "<cstdint>";
	}
	std::string get_serializer_name() const override;
	bool check_value(const EasySignedBigNum &value) override;
};

enum class FloatingPointPrecision{
	Float,
	Double,
};

class FloatingPointType : public Type{
	FloatingPointPrecision precision;
public:
	FloatingPointType(FloatingPointPrecision precision):
		precision(precision){}
	FloatingPointPrecision get_precision() const{
		return this->precision;
	}
	using Type::get_source_name;
	std::string get_source_name() const override{
		return this->get_serializer_name();
	}
	std::string get_serializer_name() const override;
};

enum class CharacterWidth{
	Narrow,
	Wide,
};

class StringType : public Type{
	CharacterWidth width;
public:
	StringType(CharacterWidth width): width(width){}
	using Type::get_source_name;
	std::string get_source_name() const override;
	const char *header() const override{
		return "<string>";
	}
	std::string get_serializer_name() const override{
		switch (this->width) {
			case CharacterWidth::Narrow:
				return "str";
			case CharacterWidth::Wide:
				return "u32str";
			default:
				throw std::exception();
		}
	}
};

class DatetimeTime : public Type{
public:
	std::string get_source_name() const override{
		return "boost::datetime";
	}
	std::string get_serializer_name() const override{
		return "datetime";
	}
};

class NestedType : public Type{
protected:
	std::shared_ptr<Type> inner;
	virtual void iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited) override{
		this->inner->iterate(callback, visited);
		Type::iterate_internal(callback, visited);
	}
	virtual void iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore) override{
		bool not_ignore = this->is_pointer_type();
		this->inner->iterate_only_public(callback, visited, not_ignore);
		Type::iterate_only_public_internal(callback, visited, do_not_ignore);
	}
public:
	NestedType(const std::shared_ptr<Type> &inner): inner(inner){}
	virtual ~NestedType(){}
	virtual void add_headers(std::set<std::string> &set) override{
		auto header = this->header();
		if (header)
			set.insert((std::string)header);
		this->inner->add_headers(set);
	}
};

class ArrayType : public NestedType{
	EasyBigNum length;
public:
	ArrayType(const std::shared_ptr<Type> &inner, const EasySignedBigNum &n);
	std::string get_source_name() const override;
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	std::string get_serializer_name() const override;
	void generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const override;
	const char *header() const override{
		return "<array>";
	}
};

class PointerType : public NestedType{
public:
	PointerType(const std::shared_ptr<Type> &inner): NestedType(inner){}
	std::string get_source_name() const override{
		return this->inner->get_source_name() + " *";
	}
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	std::string get_serializer_name() const override{
		return this->inner->get_serializer_name() + "*";
	}
	virtual bool is_pointer_type() const override{
		return true;
	}
};

class StdSmartPtrType : public NestedType{
public:
	StdSmartPtrType(const std::shared_ptr<Type> &inner): NestedType(inner){}
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	const char *header() const override{
		return "<memory>";
	}
	virtual bool is_pointer_type() const override{
		return true;
	}
};

class SharedPtrType : public StdSmartPtrType{
public:
	SharedPtrType(const std::shared_ptr<Type> &inner): StdSmartPtrType(inner){}
	std::string get_source_name() const override{
		return "std::shared_ptr<" + this->inner->get_source_name() + ">";
	}
	std::string get_serializer_name() const override{
		return "shared_ptr<" + this->inner->get_serializer_name() + ">";
	}
};

class UniquePtrType : public StdSmartPtrType{
public:
	UniquePtrType(const std::shared_ptr<Type> &inner): StdSmartPtrType(inner){}
	std::string get_source_name() const override{
		return "std::unique_ptr<" + this->inner->get_source_name() + ">";
	}
	std::string get_serializer_name() const override{
		return "unique_ptr<" + this->inner->get_serializer_name() + ">";
	}
};

class SequenceType : public NestedType{
public:
	SequenceType(const std::shared_ptr<Type> &inner): NestedType(inner){}
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
};

class VectorType : public SequenceType{
public:
	VectorType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::string get_source_name() const override{
		return "std::vector<" + this->inner->get_source_name() + ">";
	}
	const char *header() const override{
		return "<vector>";
	}
	std::string get_serializer_name() const override{
		return "vector<" + this->inner->get_serializer_name() + ">";
	}
};

class ListType : public SequenceType{
public:
	ListType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::string get_source_name() const override{
		return "std::list<" + this->inner->get_source_name() + ">";
	}
	const char *header() const override{
		return "<list>";
	}
	std::string get_serializer_name() const override{
		return "list<" + this->inner->get_serializer_name() + ">";
	}
};

class SetType : public SequenceType{
public:
	SetType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::string get_source_name() const override{
		return "std::set<" + this->inner->get_source_name() + ">";
	}
	const char *header() const override{
		return "<set>";
	}
	std::string get_serializer_name() const override{
		return "set<" + this->inner->get_serializer_name() + ">";
	}
};

class HashSetType : public SequenceType{
public:
	HashSetType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::string get_source_name() const override{
		return "std::unordered_set<" + this->inner->get_source_name() + ">";
	}
	const char *header() const override{
		return "<unordered_set>";
	}
	std::string get_serializer_name() const override{
		return "unordered_set<" + this->inner->get_serializer_name() + ">";
	}
};

class OptionalType : public NestedType{
public:
	OptionalType(const std::shared_ptr<Type> &inner): NestedType(inner){}
	std::string get_source_name() const override{
		return "std::optional<" + this->inner->get_source_name() + ">";
	}
	const char *header() const override{
		return "<optional>";
	}
	std::string get_serializer_name() const override{
		return "optional<" + this->inner->get_serializer_name() + ">";
	}
	CppVersion minimum_cpp_version() const override{
		return CppVersion::Cpp2017;
	}
};

class PairType : public Type{
protected:
	std::shared_ptr<Type> first,
		second;
	void iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited) override{
		this->first->iterate(callback, visited);
		this->second->iterate(callback, visited);
		Type::iterate_internal(callback, visited);
	}
	void iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore) override{
		bool not_ignore = this->is_pointer_type();
		this->first->iterate_only_public(callback, visited, not_ignore);
		this->second->iterate_only_public(callback, visited, not_ignore);
		Type::iterate_only_public_internal(callback, visited, do_not_ignore);
	}
public:
	PairType(const std::shared_ptr<Type> &first, const std::shared_ptr<Type> &second):
		first(first),
		second(second){}
	virtual ~PairType(){}
};

class AssociativeArrayType : public PairType{
public:
	AssociativeArrayType(const std::shared_ptr<Type> &first, const std::shared_ptr<Type> &second):
		PairType(first, second){}
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
};

class MapType : public AssociativeArrayType{
public:
	MapType(const std::shared_ptr<Type> &first, const std::shared_ptr<Type> &second):
		AssociativeArrayType(first, second){}
	std::string get_source_name() const override{
		return "std::map<" + this->first->get_source_name() + ", " + this->second->get_source_name() + ">";
	}
	const char *header() const override{
		return "<map>";
	}
	std::string get_serializer_name() const override{
		return "map<" + this->first->get_serializer_name() + "," + this->second->get_serializer_name() + ">";
	}
};

class HashMapType : public AssociativeArrayType{
public:
	HashMapType(const std::shared_ptr<Type> &first, const std::shared_ptr<Type> &second):
		AssociativeArrayType(first, second){}
	std::string get_source_name() const override{
		return "std::unordered_map<" + this->first->get_source_name() + ", " + this->second->get_source_name() + ">";
	}
	const char *header() const override{
		return "<unordered_map>";
	}
	std::string get_serializer_name() const override{
		return "unordered_map<" + this->first->get_serializer_name() + "," + this->second->get_serializer_name() + ">";
	}
};

class Object{
	std::shared_ptr<Type> type;
	std::string name;
public:
	Object(const std::shared_ptr<Type> &type, const std::string &name):
		type(type),
		name(name){}
	virtual ~Object(){}
	virtual std::string output() const{
		return this->type->get_source_name(this->name);
	}
	virtual void add_headers(std::set<std::string> &set){
		this->type->add_headers(set);
	}
	std::shared_ptr<const Type> get_type() const{
		return this->type;
	}
	std::shared_ptr<Type> get_type(){
		return this->type;
	}
	std::string get_name() const{
		return this->name;
	}
};

enum class Accessibility{
	Public = 0,
	Protected = 1,
	Private = 2,
};

inline const char *to_string(Accessibility a){
	switch (a){
		case Accessibility::Public:
			return "public";
		case Accessibility::Protected:
			return "protected";
		case Accessibility::Private:
			return "private";
	}
	return "";
}

class ClassElement{
public:
	virtual ~ClassElement(){}
	virtual std::string output() const = 0;
	virtual bool needs_semicolon() const{
		return true;
	}
	virtual CppVersion minimum_cpp_version() const{
		return CppVersion::Undefined;
	}
};

class ClassMember : public Object, public ClassElement{
	Accessibility accessibility;
public:
	ClassMember(const std::shared_ptr<Type> &type, const std::string &name, Accessibility accessibility):
		Object(type, name),
		accessibility(accessibility){}
	std::string output() const override{
		return (std::string)to_string(this->accessibility) + ": " + this->Object::output();
	}
	Accessibility get_accessibility() const{
		return this->accessibility;
	}
	CppVersion minimum_cpp_version() const override{
		return this->get_type()->minimum_cpp_version();
	}
};

class VerbatimBlock : public ClassElement{
	std::string content;
public:
	VerbatimBlock(std::string content): content(std::move(content)){}
	std::string output() const override{
		return this->content;
	}
};

class UserClass;

struct InheritanceEdge{
	std::shared_ptr<UserClass> Class;
	bool Virtual = false;
	InheritanceEdge(const std::shared_ptr<UserClass> &c): Class(c){}
	InheritanceEdge(const InheritanceEdge &) = default;
};

class UserType : public Type, public CppElement{
protected:
	std::string name;
	std::vector<std::string> _namespace;

	std::string generate_namespace(bool last_colons = true) const;
public:
	UserType(CppFile &file, std::string name, std::vector<std::string> _namespace);
	virtual ~UserType(){}
	std::string get_serializer_name() const override{
		return this->name;
	}
	std::string get_source_name() const override{
		return this->generate_namespace() + this->name;
	}
	const std::string &get_name() const{
		return this->name;
	}
};

class UserClass : public UserType{
	std::vector<InheritanceEdge> base_classes;
	std::vector<std::shared_ptr<ClassElement>> elements;
	bool adding_headers;
	static unsigned next_type_id;
	bool abstract_type;
	bool inherit_Serializable_virtually = false;
	std::uint32_t diamond_detection = 0;
	std::vector<UserClass *> all_base_classes;
	int trivial_class = -1;
	bool default_destructor = true;
protected:
	void iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited) override;
	void iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore) override;

public:

	UserClass(CppFile &file, std::string name, std::vector<std::string> _namespace, bool is_abstract = false)
		: UserType(file, std::move(name), std::move(_namespace))
		, adding_headers(false)
		, abstract_type(is_abstract)
	{}
	void add_base_class(const std::shared_ptr<UserClass> &base){
		this->base_classes.emplace_back(base);
	}
	void add_element(const std::shared_ptr<ClassElement> &member){
		this->elements.emplace_back(member);
	}
	std::string generate_header() const override;
	std::string generate_destructor() const;
	void generate_source(std::ostream &stream) const;
	void add_headers(std::set<std::string> &set) override;
	void generate_get_object_node2(std::ostream &) const;
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	void generate_serialize(std::ostream &) const;
	void generate_get_metadata(std::ostream &) const;
	void generate_deserializer(std::ostream &) const;
	const TypeHash &get_type_hash() override{
		if (!this->type_hash){
			std::stringstream stream;
			stream << "version" << SERIALIZER_VERSION << this->base_get_type_string();
			this->type_hash = TypeHash(stream.str());
		}
		return this->type_hash;
	}
	std::string base_get_type_string() const override;
	void generate_get_type_hash(std::ostream &) const;
#define DEFINE_GENERATE_OVERLOAD(x) \
	std::string x() const{          \
		std::stringstream stream;   \
		this->x(stream);            \
		return stream.str();        \
	}
	DEFINE_GENERATE_OVERLOAD(generate_get_object_node2)
	DEFINE_GENERATE_OVERLOAD(generate_serialize)
	DEFINE_GENERATE_OVERLOAD(generate_get_type_hash)
	DEFINE_GENERATE_OVERLOAD(generate_get_metadata)
	DEFINE_GENERATE_OVERLOAD(generate_deserializer)
	void generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const override;
	void generate_rollbacker(std::ostream &stream, const char *pointer_name) const override;
	bool get_is_serializable() const override;
	bool is_abstract() const override{
		return this->abstract_type;
	}
	void set_abstract(bool v){
		this->abstract_type = v;
	}
	bool is_serializable() const override{
		return true;
	}
	void walk_class_hierarchy(const std::function<bool(UserClass &)> &callback);
	void mark_virtual_inheritances(std::uint32_t);
	void mark_virtual_inheritances(std::uint32_t, const std::vector<std::uint32_t> &);
	bool is_subclass_of(const UserClass &other) const;
	const std::vector<UserClass *> &get_all_base_classes();
	bool is_trivial_class();
	CppVersion minimum_cpp_version() const override;
	std::string generate_forward_declaration() const override;
	void no_default_destructor();
};

class UserEnum : public UserType{
	std::uint32_t id;
	std::shared_ptr<Type> underlying_type;
	std::map<std::string, EasySignedBigNum> members_by_name;
	std::map<std::string, std::vector<std::string>> additional_strings_by_name;
	std::map<EasySignedBigNum, std::string> members_by_value;
	mutable std::optional<size_t> cached_max_additional_strings;

	std::string generate_members() const;
	std::string generate_stringifier_cases() const;
	std::string get_stringifier_return_type() const;
	std::string get_stringifier_signature() const;
	size_t get_max_additional_strings() const;
	std::string get_stringifier_default_value() const;
public:
	UserEnum(CppEvaluationState &, std::string name, std::vector<std::string> _namespace, std::shared_ptr<Type> underlying_type);
	std::string base_get_type_string() const override;
	std::string get_serializer_name() const override{
		return this->name;
	}
	void set(std::string name, EasySignedBigNum value, std::vector<std::string> additional_strings);
	std::string generate_forward_declaration() const override;
	std::string generate_header() const override;
	std::shared_ptr<Type> get_underlying_type() override{
		return this->underlying_type;
	}
	std::uint32_t get_id() const{
		return this->id;
	}
	std::vector<EasySignedBigNum> get_valid_values() const;
	std::string generate_stringifier();
};

class CppFile{
	std::string name;
	std::vector<std::shared_ptr<CppElement>> elements;
	std::unordered_map<std::string, std::shared_ptr<UserClass>> classes;
	std::unordered_map<std::string, std::shared_ptr<UserEnum>> enums;
	std::vector<std::pair<std::string, std::string>> type_mappings;
	typedef std::map<unsigned, Type *> type_map_t;
	type_map_t type_map;

	std::string generate_sizes();
	std::string generate_deserializers();
	std::string generate_rollbackers();
	std::string generate_dynamic_casts();
	std::string generate_cast_cases(UserClass &, const char *format);
	std::string generate_raw_pointer_cast_cases(UserClass &);
	std::string generate_shared_ptr_cast_cases(UserClass &);
	std::string generate_unique_ptr_cast_cases(UserClass &);
	std::string generate_allocator_cases(const char *format);
	std::string generate_raw_pointer_allocator_cases();
	std::string generate_shared_pointer_allocator_cases();
	std::string generate_unique_ptr_allocator_cases();
	std::string generate_cast_categories(unsigned max_type);
	std::string generate_type_comments();
	std::string generate_type_map();
	std::string get_id_hashes_name();
	std::string generate_enum_checkers();
	std::string generate_source1();
	std::string generate_static_get_type_id();
public:
	CppFile(const std::string &name): name(name){}
	void add_element(const std::shared_ptr<CppElement> &element){
		this->elements.push_back(element);
		if (auto Class = std::dynamic_pointer_cast<UserClass>(element))
			this->classes[Class->get_name()] = Class;
		else if (auto Enum = std::dynamic_pointer_cast<UserEnum>(element))
			this->enums[Enum->get_name()] = Enum;
	}
	const std::string &get_name() const{
		return this->name;
	}
	const decltype(classes) &get_classes() const{
		return this->classes;
	}
	void add_type_mapping(std::string src, std::string dst){
		this->type_mappings.emplace_back(std::move(src), std::move(dst));
	}
	void generate_header();
	void generate_source();
	std::string generate_allocator();
	std::string generate_constructor();
	std::string generate_rollbacker();
	std::string generate_is_serializable();
	std::string generate_dynamic_cast();
	std::string generate_generic_pointer_classes();
	std::string generate_generic_pointer_class_implementations();
	std::string generate_generic_pointer_classes_and_implementations();
	std::string generate_pointer_allocator();
	std::string generate_cast_categorizer();
	std::string generate_enum_checker();
	std::string generate_enum_stringifiers();
	std::string generate_get_metadata();
	std::uint32_t assign_type_ids();
	void mark_virtual_inheritances();
	CppVersion minimum_cpp_version() const;
};

class UserInclude : public CppElement{
	std::string include;
	bool relative;
public:
	UserInclude(CppFile &file, const std::string &include, bool relative): CppElement(file), include(include), relative(relative){}
	std::string generate_header() const override;
};
