#ifndef HAVE_PRECOMPILED_HEADERS
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
#include <boost/bimap.hpp>
#include <boost/bimap/set_of.hpp>
#include <random>
#endif
#include "typehash.h"

extern std::mt19937_64 rng;

class CppElement{
public:
	virtual ~CppElement(){}
	virtual std::ostream &output(std::ostream &) const = 0;
};

enum class CallMode{
	TransformAndAdd,
	TransformAndReturn,
	AddVerbatim,
};
typedef const std::function<std::string (const std::string &, CallMode)> generate_pointer_enumerator_callback_t;

class Type{
	std::uint32_t type_id;
	std::unique_ptr<TypeHash> type_hash;
protected:
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
	virtual std::ostream &output(std::ostream &stream) const = 0;
	virtual std::ostream &output(std::ostream &stream, const std::string &name) const{
		this->output(stream);
		return stream << " " << name;
	}
	virtual const char *header() const{
		return nullptr;
	}
	virtual void add_headers(std::set<std::string> &set) const{
		auto header = this->header();
		if (header)
			set.insert((std::string)header);
	}
	virtual void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{}
	virtual void generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const;
	virtual void generate_rollbacker(std::ostream &stream, const char *pointer_name) const;
	virtual void generate_is_serializable(std::ostream &stream) const;
	std::uint32_t get_type_id() const{
		return this->type_id;
	}
	virtual std::string get_type_string() const = 0;
	const TypeHash &get_type_hash(){
		if (!this->type_hash)
			this->type_hash.reset(new TypeHash(this->get_type_string()));
		return *this->type_hash;
	}
	const TypeHash &get_type_hash() const{
		if (!this->type_hash)
			abort();
		return *this->type_hash;
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
};

#if 1
inline std::ostream &operator<<(std::ostream &stream, Type &t){
	return t.output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, Type *t){
	return t->output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, std::shared_ptr<Type> t){
	return t->output(stream);
}
#else
template <typename T>
std::ostream &operator<<(std::ostream &stream, T &t){
	return t.output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, T *t){
	return t->output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, std::shared_ptr<T> t){
	return t->output(stream);
}
#endif

class BoolType : public Type{
public:
	std::ostream &output(std::ostream &stream) const override{
		return stream << "bool";
	}
	std::string get_type_string() const override{
		return "b";
	}
};

class IntegerType : public Type{
	bool signedness;
	//valid sizes = 0, 1, 2, 3
	unsigned size;

	static std::shared_ptr<IntegerType> creator_helper(size_t i){
		return std::shared_ptr<IntegerType>(new IntegerType(i & 1, i / 2));
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
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::" << (!this->signedness ? "u" : "") << "int" << (8 << this->size) << "_t";
	}
	const char *header() const override{
		return "<cstdint>";
	}
	std::string get_type_string() const override;
};

class StringType : public Type{
	bool wide;
public:
	StringType(bool wide): wide(wide){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::" << (this->wide ? "w" : "") << "string";
	}
	const char *header() const override{
		return "<string>";
	}
	std::string get_type_string() const override{
		return this->wide ? "wstr" : "str";
	}
};

class DatetimeTime : public Type{
public:
	std::ostream &output(std::ostream &stream) const override{
		return stream << "boost::datetime";
	}
	std::string get_type_string() const override{
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
	virtual void add_headers(std::set<std::string> &set) const override{
		auto header = this->header();
		if (header)
			set.insert((std::string)header);
		this->inner->add_headers(set);
	}
};

class ArrayType : public NestedType{
	size_t length;
public:
	ArrayType(const std::shared_ptr<Type> &inner, size_t n): NestedType(inner), length(n){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << this->inner << "[" << this->length << "]";
	}
	std::ostream &output(std::ostream &stream, const std::string &name) const override{
		return stream << this->inner << " " << name << "[" << this->length << "]";
	}
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	std::string get_type_string() const override;
	void generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const override;
};

class PointerType : public NestedType{
public:
	PointerType(const std::shared_ptr<Type> &inner): NestedType(inner){}
	virtual std::ostream &output(std::ostream &stream) const override{
		return stream << this->inner << " *";
	}
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	std::string get_type_string() const override{
		return this->inner->get_type_string() + "*";
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
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::shared_ptr<" << this->inner << ">";
	}
	std::string get_type_string() const override{
		return "shared_ptr<" + this->inner->get_type_string() + ">";
	}
};

class UniquePtrType : public StdSmartPtrType{
public:
	UniquePtrType(const std::shared_ptr<Type> &inner): StdSmartPtrType(inner){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::unique_ptr<" << this->inner << ">";
	}
	std::string get_type_string() const override{
		return "unique_ptr<" + this->inner->get_type_string() + ">";
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
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::vector<" << this->inner << ">";
	}
	const char *header() const override{
		return "<vector>";
	}
	std::string get_type_string() const override{
		return "vector<" + this->inner->get_type_string() + ">";
	}
};

class ListType : public SequenceType{
public:
	ListType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::list<" << this->inner << ">";
	}
	const char *header() const override{
		return "<list>";
	}
	std::string get_type_string() const override{
		return "list<" + this->inner->get_type_string() + ">";
	}
};

class SetType : public SequenceType{
public:
	SetType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::set<" << this->inner << ">";
	}
	const char *header() const override{
		return "<set>";
	}
	std::string get_type_string() const override{
		return "set<" + this->inner->get_type_string() + ">";
	}
};

class HashSetType : public SequenceType{
public:
	HashSetType(const std::shared_ptr<Type> &inner): SequenceType(inner){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::unordered_set<" << this->inner << ">";
	}
	const char *header() const override{
		return "<unordered_set>";
	}
	std::string get_type_string() const override{
		return "unordered_set<" + this->inner->get_type_string() + ">";
	}
};

class PairType : public Type{
protected:
	std::shared_ptr<Type> first,
		second;
	virtual void iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited) override{
		this->first->iterate(callback, visited);
		this->second->iterate(callback, visited);
		Type::iterate_internal(callback, visited);
	}
	virtual void iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore) override{
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
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::map<" << this->first << ", " << this->second << ">";
	}
	const char *header() const override{
		return "<map>";
	}
	std::string get_type_string() const override{
		return "map<" + this->first->get_type_string() + "," + this->second->get_type_string() + ">";
	}
};

class HashMapType : public AssociativeArrayType{
public:
	HashMapType(const std::shared_ptr<Type> &first, const std::shared_ptr<Type> &second):
		AssociativeArrayType(first, second){}
	std::ostream &output(std::ostream &stream) const override{
		return stream << "std::unordered_map<" << this->first << ", " << this->second << ">";
	}
	const char *header() const override{
		return "<unordered_map>";
	}
	std::string get_type_string() const override{
		return "unordered_map<" + this->first->get_type_string() + "," + this->second->get_type_string() + ">";
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
	virtual std::ostream &output(std::ostream &stream) const{
		return this->type->output(stream, this->name);
	}
	virtual void add_headers(std::set<std::string> &set) const{
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

#if 1
inline std::ostream &operator<<(std::ostream &stream, Object &t){
	return t.output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, Object *t){
	return t->output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, std::shared_ptr<Object> t){
	return t->output(stream);
}
#endif

#if 1
inline std::ostream &operator<<(std::ostream &stream, CppElement &t){
	return t.output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, CppElement *t){
	return t->output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, std::shared_ptr<CppElement> t){
	return t->output(stream);
}
#endif

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
	virtual std::ostream &output(std::ostream &stream) const = 0;
	virtual bool needs_semicolon() const{
		return true;
	}
};

#if 1
inline std::ostream &operator<<(std::ostream &stream, ClassElement &t){
	return t.output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, ClassElement *t){
	return t->output(stream);
}

inline std::ostream &operator<<(std::ostream &stream, std::shared_ptr<ClassElement> t){
	return t->output(stream);
}
#endif

class ClassMember : public Object, public ClassElement{
	Accessibility accessibility;
public:
	ClassMember(const std::shared_ptr<Type> &type, const std::string &name, Accessibility accessibility):
		Object(type, name),
		accessibility(accessibility){}
	std::ostream &output(std::ostream &stream) const override{
		stream << to_string(this->accessibility) << ": ";
		return Object::output(stream);
	}
	Accessibility get_accessibility() const{
		return this->accessibility;
	}
};

class UserClass : public Type, public CppElement{
	std::vector<std::shared_ptr<UserClass> > base_classes;
	std::vector<std::shared_ptr<ClassElement> > elements;
	std::string name;
	bool adding_headers;
	static unsigned next_type_id;
protected:
	virtual void iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited) override;
	virtual void iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore) override;
public:
	UserClass(const std::string &name):
		adding_headers(false),
		name(name){}
	void add_base_class(const std::shared_ptr<UserClass> &base){
		this->base_classes.push_back(base);
	}
	void add_element(const std::shared_ptr<ClassElement> &member){
		this->elements.push_back(member);
	}
	std::ostream &output(std::ostream &stream) const override{
		return stream << this->name;
	}
	void generate_header(std::ostream &stream) const;
	void generate_source(std::ostream &stream) const;
	virtual void add_headers(std::set<std::string> &set);
	const std::string &get_name() const{
		return this->name;
	}
	void generate_get_object_node2(std::ostream &) const;
	void generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const override;
	void generate_serialize(std::ostream &) const;
	void generate_get_metadata(std::ostream &) const;
	void generate_deserializer(std::ostream &) const;
	std::string get_type_string() const override{
		return this->name;
	}
	std::string base_get_type_string() const;
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
	void generate_is_serializable(std::ostream &stream) const override;
};

class CppFile{
	std::string name;
	std::vector<std::shared_ptr<CppElement> > elements;
	std::unordered_map<std::string, std::shared_ptr<UserClass> > classes;
	typedef std::map<unsigned, Type *> type_map_t;
	type_map_t type_map;
	void assign_type_ids();
public:
	CppFile(const std::string &name): name(name){}
	void add_element(const std::shared_ptr<CppElement> &element){
		this->elements.push_back(element);
		auto Class = std::dynamic_pointer_cast<UserClass>(element);
		if (Class)
			this->classes[Class->get_name()] = Class;
	}
	const std::string &get_name() const{
		return this->name;
	}
	const decltype(classes) &get_classes() const{
		return this->classes;
	}
	void generate_header();
	void generate_source();
	void generate_aux();
	void generate_allocator(std::ostream &);
	void generate_constructor(std::ostream &);
	void generate_rollbacker(std::ostream &);
	void generate_is_serializable(std::ostream &);
};

class UserInclude : public CppElement, public ClassElement{
	std::string include;
public:
	UserInclude(const std::string &include): include(include){}
	std::ostream &output(std::ostream &stream) const{
		stream << "#include " << this->include;
		return stream;
	}
	bool needs_semicolon() const override{
		return false;
	}
};
