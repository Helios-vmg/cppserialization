#include "stdafx.h"
#include "las.h"
#include "util.h"
#include "variable_formatter.h"
#include "typehash.h"
#ifndef HAVE_PRECOMPILED_HEADERS
#include <fstream>
#include <sstream>
#include <array>
#include <iomanip>
#include <boost/format.hpp>
#endif

std::mt19937_64 rng;
std::uint64_t random_function_name = 0;

void get_randomized_function_name(std::string &name, const char *custom_part){
	if (name.size())
		return;
	if (!random_function_name)
		random_function_name = rng();
	std::stringstream stream;
	stream << custom_part << std::hex << std::setw(16) << std::setfill('0') << random_function_name;
	name = stream.str();
}

#define DEFINE_get_X_function_name(x)                            \
	std::string x##_function_name;                               \
	std::string get_##x##_function_name(){                       \
		get_randomized_function_name(x##_function_name, #x "_"); \
		return x##_function_name;                                \
	}

DEFINE_get_X_function_name(get_metadata)
DEFINE_get_X_function_name(allocator)
DEFINE_get_X_function_name(constructor)
DEFINE_get_X_function_name(rollbacker)
DEFINE_get_X_function_name(is_serializable)

std::string IntegerType::get_type_string() const{
	std::stringstream stream;
	stream << (this->signedness ? 's' : 'u') << (8 << this->size);
	return stream.str();
}

std::string ArrayType::get_type_string() const{
	std::stringstream stream;
	stream << "std::array< " << this->inner->get_type_string() << ", " << this->length << ">";
	return stream.str();
}

void ArrayType::generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const{
	stream << "{\n";
	this->output(stream);
	stream << " *temp = (";
	this->output(stream);
	stream << " *)" << pointer_name << ";\n"
		"for (size_t i = 0; i != " << this->length << "; i++){\n"
		"new (temp + i) ";
	this->output(stream);
	stream << ";\n"
		<< deserializer_name << ".deserialize(temp[i]);\n"
		"}\n"
		"}\n";
}

template <typename T>
void generate_for_collection(variable_formatter &format, const std::string &next_name, const std::initializer_list<T> &list, generate_pointer_enumerator_callback_t &callback){
	bool any = false;
	std::string accum;
	auto subcallback = [&any, &accum, &callback](const std::string &input, CallMode mode) -> std::string{
		if (mode == CallMode::TransformAndReturn)
			return callback(input, mode);

		if (mode == CallMode::AddVerbatim){
			any = true;
			accum += input;
			return std::string();
		}

		any = true;
		accum += callback(input, CallMode::TransformAndReturn);
		return std::string();
	};
	for (auto &i : list)
		i->generate_pointer_enumerator(subcallback, next_name);
	if (any)
		callback(format << "contents" << accum, CallMode::AddVerbatim);
}

void Type::generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const{
	stream << "{\n";
	this->output(stream);
	stream << " *temp = (";
	this->output(stream);
	stream << " *)" << pointer_name << ";\n"
		"new (temp) ";
	this->output(stream);
	stream << ";\n"
		<< deserializer_name << ".deserialize(*temp);\n"
		"}\n";
}

void Type::generate_rollbacker(std::ostream &stream, const char *pointer_name) const{
}

void Type::generate_is_serializable(std::ostream &stream) const{
	stream << "return false;\n";
}

void ArrayType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	static const char *format = 
		"for (size_t {index} = 0; {index} != {length}; {index}++){{"
			"{contents}"
		"}}";
	variable_formatter vf(format);
	auto index = get_unique_varname();
	vf << "index" << index << "length" << this->length;
	std::string next_name = variable_formatter("({name})[{index}]") << "name" << this_name << "index" << index;
	generate_for_collection(vf, next_name, { this->inner }, callback);
}

void PointerType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	std::stringstream stream;
	stream << this_name << ", ";
	if (!this->inner->is_serializable())
		stream << "static_get_type_id<" << this->inner << ">::value";
	else
		stream << "(" << this_name << ")->get_type_id()";
	callback(this_name, CallMode::TransformAndAdd);
}

void StdSmartPtrType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	std::stringstream stream;
	stream << "(" << this_name << ").get(), ";
	if (!this->inner->is_serializable())
		stream << "static_get_type_id<" << this->inner << ">::value";
	else
		stream << "(" << this_name << ")->get_type_id()";
	callback(stream.str(), CallMode::TransformAndAdd);
}

void SequenceType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	static const char *format = 
		"for (const auto &{index} : {name}){{"
			"{contents}"
		"}}";
	variable_formatter vf(format);
	auto index = get_unique_varname();
	vf << "index" << index << "name" << this_name;
	generate_for_collection(vf, index, { this->inner }, callback);
}

void AssociativeArrayType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	static const char *format = 
		"for (const auto &{index} : {name}){{"
			"{contents}"
		"}}";
	variable_formatter vf(format);
	auto index = get_unique_varname();
	vf << "index" << index << "name" << this_name;
	generate_for_collection(vf, index, { this->first, this->second }, callback);
}

unsigned UserClass::next_type_id = 0;

struct autoset{
	bool &b;
	autoset(bool &b): b(b){
		b = true;
	}
	~autoset(){
		this->b = false;
	}
};

void UserClass::add_headers(std::set<std::string> &set){
	if (this->adding_headers)
		return;
	autoset b(this->adding_headers);
	for (auto &bc : this->base_classes)
		bc->add_headers(set);
	for (auto &e : this->elements){
		auto member = std::dynamic_pointer_cast<ClassMember>(e);
		if (!member)
			continue;
		member->add_headers(set);
	}
}

void UserClass::generate_header(std::ostream &stream) const{
	stream << "class " << this->name << " : ";
	if (this->base_classes.size() > 0){
		bool first = true;
		for (auto &base : this->base_classes){
			if (!first)
				stream << ", ";
			else
				first = false;
			stream << "public " << base->name;
		}
	}else
		stream << "public Serializable";
	stream << "{\n";
	for (auto &el : this->elements)
		stream << "" << el << (el->needs_semicolon() ? ";\n" : "\n");

	static const char *format =
		"public:\n"
		"{name}(DeserializerStream &);\n"
		"virtual ~{name}();\n"
		"virtual void get_object_node(std::vector<ObjectNode> &) const override;\n"
		"virtual void serialize(SerializerStream &) const override;\n"
		"virtual std::uint32_t get_type_id() const override;\n"
		"virtual TypeHash get_type_hash() const override;\n"
		"virtual std::shared_ptr<SerializableMetadata> get_metadata() const override;\n"
		"static std::shared_ptr<SerializableMetadata> static_get_metadata();\n"
		"virtual void rollback_deserialization() override;\n"
		"}};\n";
	stream << (std::string)(variable_formatter(format) << "name" << this->name);
}

template <typename T>
size_t string_length(const T *str){
	size_t ret = 0;
	for (; str[ret]; ret++);
	return ret;
}

template <typename T>
size_t string_length(const std::basic_string<T> &str){
	return str.size();
}

template <typename T, typename T2, typename T3>
std::basic_string<T> replace_all(std::basic_string<T> s, const T2 &search, const T3 &replacement){
	size_t pos = 0;
	auto length = string_length(search);
	while ((pos = s.find(search)) != s.npos){
		auto s2 = s.substr(0, pos);
		s2 += replacement;
		s2 += s.substr(pos + length);
		s = s2;
	}
	return s;
}

void UserClass::generate_get_object_node2(std::ostream &stream) const{
	for (auto &i : this->base_classes)
		stream << i->get_name() << "::get_object_node(v);\n";

	auto callback = [&stream](const std::string &s, CallMode mode){
		std::string addend;
		if (mode != CallMode::AddVerbatim){
			auto replaced = replace_all(s, "(*this).", "this->");
			addend = variable_formatter("v.push_back(::get_object_node({0}));\n") << "0" << replaced;
			if (mode == CallMode::TransformAndReturn)
				return addend;
		}else
			addend = s;
		stream << addend;
		return std::string();
	};
	this->generate_pointer_enumerator(callback, "*this");
}

void UserClass::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	auto next_name = "(" + this_name + ").";
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		auto type = casted->get_type();
		type->generate_pointer_enumerator(callback, next_name + casted->get_name());
	}
}

void UserClass::generate_serialize(std::ostream &stream) const{
	for (auto &b : this->base_classes)
		stream << b->get_name() << "::serialize(ss);\n";
	
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		stream << "ss.serialize(this->" << casted->get_name() << ");\n";
	}
}

void UserClass::generate_get_type_hash(std::ostream &stream) const{
	stream << "return TypeHash(" << this->get_type_hash().to_string() << ");";
}

void UserClass::generate_get_metadata(std::ostream &stream) const{
	stream << "return " << get_get_metadata_function_name() << "();\n";
}

void UserClass::generate_deserializer(std::ostream &stream) const{
	bool first = true;
	for (auto &b : this->base_classes){
		if (!first)
			stream << ", ";
		else{
			stream << ": ";
			first = false;
		}
		stream << b->get_name() << "(ds)\n";
	}
	
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		if (!first)
			stream << ", ";
		else{
			stream << ": ";
			first = false;
		}
		stream << casted->get_name() << "(";
		auto type = casted->get_type();
		if (std::dynamic_pointer_cast<UserClass>(type))
			stream << "ds";
		else
			stream << "proxy_constructor<" << type << ">(ds)";
		stream << ")\n";
	}
	stream << "{}";
}

void UserClass::generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const{
	stream << "{\n";
	this->output(stream);
	stream << " *temp = (";
	this->output(stream);
	stream << " *)" << pointer_name << ";\n"
		"new (temp) ";
	this->output(stream);
	stream << "(" << deserializer_name << ");\n"
		"}\n";
}

void UserClass::generate_rollbacker(std::ostream &stream, const char *pointer_name) const{
	stream << "{\n";
	this->output(stream);
	stream << " *temp = (";
	this->output(stream);
	stream << " *)" << pointer_name << ";\n"
		"temp->rollback_deserialization();\n"
		"}\n";
}

void UserClass::generate_is_serializable(std::ostream &stream) const{
	stream << "return true;\n";
}

void UserClass::generate_source(std::ostream &stream) const{
	static const char *format =
		"{name}::{name}(DeserializerStream &ds)\n"
		"{ctor}"
		"\n"
		"\n"
		"void {name}::get_object_node(std::vector<ObjectNode> &v) const{{\n"
		"{gon}"
		"}}\n"
		"\n"
		"void {name}::serialize(SerializerStream &ss) const{{\n"
		"{ser}"
		"}}\n"
		"\n"
		"std::uint32_t {name}::get_type_id() const{{\n"
		"{gti}"
		"}}\n"
		"\n"
		"TypeHash {name}::get_type_hash() const{{\n"
		"{gth}"
		"}}\n"
		"\n"
		"std::shared_ptr<SerializableMetadata> {name}::get_metadata() const{{\n"
		"return this->static_get_metadata();\n"
		"}}\n"
		"\n"
		"std::shared_ptr<SerializableMetadata> {name}::static_get_metadata(){{\n"
		"{gmd}"
		"}}\n"
		"\n";
	variable_formatter vf = format;
	vf
		<< "name" << this->name
		<< "ctor" << this->generate_deserializer()
		<< "gon" << this->generate_get_object_node2()
		<< "ser" << this->generate_serialize()
		<< "gti" << ("return " + utoa(this->get_type_id()) + ";")
		<< "gth" << this->generate_get_type_hash()
		<< "gmd" << this->generate_get_metadata();
	stream << (std::string)vf;
}

void UserClass::iterate_internal(iterate_callback_t &callback, std::set<Type *> &visited){
	for (auto &b : this->base_classes)
		b->iterate(callback, visited);
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		casted->get_type()->iterate(callback, visited);
	}
	Type::iterate_internal(callback, visited);
}

void UserClass::iterate_only_public_internal(iterate_callback_t &callback, std::set<Type *> &visited, bool do_not_ignore){
	for (auto &b : this->base_classes)
		b->iterate_only_public(callback, visited, false);
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		casted->get_type()->iterate_only_public(callback, visited, false);
	}
	Type::iterate_only_public_internal(callback, visited, true);
}

std::string UserClass::base_get_type_string() const{
	std::stringstream stream;
	stream << '{' << this->name;
	for (auto &b : this->base_classes)
		stream << ':' << b->base_get_type_string();
	stream << '(';
	bool first = true;
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		if (!first)
			stream << ',';
		else
			first = false;
		stream << '(' << (int)casted->get_accessibility() << ',' << casted->get_type()->get_type_string() << ',' << casted->get_name() << ')';
	}
	stream << ")}";
	return stream.str();
}

void CppFile::assign_type_ids(){
	if (this->type_map.size())
		return;

	std::uint32_t id = 1;

	std::map<TypeHash, std::pair<unsigned, Type *> > type_map;
	auto callback = [&type_map, &id](Type &t, std::uint32_t &type_id){
		auto &hash = t.get_type_hash();
		auto it = type_map.find(hash);
		if (it != type_map.end())
			return;
		type_id = id;
		type_map[hash] = std::make_pair(id, &t);
		id++;
	};

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (!Class)
			continue;
		Class->iterate_only_public(callback);
	}
	for (auto &kv : type_map)
		this->type_map[kv.second.first] = kv.second.second;
}

void CppFile::generate_header(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".generated.h").c_str());

	file <<
		"#ifdef _MSC_VER\n"
		"#pragma once\n"
		"#endif\n"
		"\n";

	std::set<std::string> includes;
	for (auto &c : this->classes)
		c.second->add_headers(includes);
	for (auto &h : includes)
		file << "#include " << h << std::endl;
	file <<
		"#include \"" << this->name << ".aux.h\"\n"
		"#include \"serialization_utils.h\"\n"
		"#include \"Serializable.h\"\n"
		"#include \"SerializerStream.h\"\n"
		"#include \"DeserializerStream.h\"\n"
		"\n";

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (Class)
			file << "class " << Class->get_name() << ";\n";
	}

	file << std::endl;

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (Class){
			Class->generate_header(file);
			auto ts = Class->base_get_type_string();
			std::cout << ts << std::endl
				<< TypeHash(ts).to_string() << std::endl;
		}else
			file << e;
		file << std::endl;
	}
}

std::string generate_get_metadata_signature(){
	return "std::shared_ptr<SerializableMetadata> " + get_get_metadata_function_name() + "()";
}

void CppFile::generate_source(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".generated.cpp").c_str());
	file << "#include \"" << this->get_name() << ".generated.h\"\n"
		"#include <utility>\n"
		"\n"
		"extern std::pair<std::uint32_t, TypeHash> " << this->get_name() << "_id_hashes[];\n"
		"extern size_t " << this->get_name() << "_id_hashes_length;\n"
		"\n"
		<< generate_get_metadata_signature() << ";\n"
		"\n"
		"template <typename T>\n"
		"struct static_get_type_id{};\n"
		"\n";

	for (auto &kv : this->type_map){
		file <<
			"template <>\n"
			"struct static_get_type_id<" << *kv.second << ">{\n"
			"static const std::uint32_t value = " << kv.second->get_type_id() << ";\n"
			"};\n"
			"\n";
	}

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (!Class)
			continue;
		Class->generate_source(file);
		file << std::endl;
	}
}

void CppFile::generate_allocator(std::ostream &stream){
	stream << "void *" << get_allocator_function_name() << "(std::uint32_t type){\n"
		"switch (type){\n";
	for (auto &kv : this->type_map){
		stream << "case " << kv.first << ":\n";
		if (kv.second->is_abstract()){
			stream << "return nullptr;\n";
			continue;
		}
		
		stream << "return ::operator new (sizeof(";
		kv.second->output(stream);
		stream << "));\n";
	}
	stream <<
		"}\n"
		"return nullptr;"
		"}\n";
}

void CppFile::generate_constructor(std::ostream &stream){
	stream << "void " << get_constructor_function_name() << "(std::uint32_t type, void *s, DeserializerStream &ds){\n"
		"switch (type){\n";
	for (auto &kv : this->type_map){
		stream << "case " << kv.first << ":\n";
		if (!kv.second->is_abstract())
			kv.second->generate_deserializer(stream, "ds", "s");
		stream << "break;\n";
	}
	stream <<
		"}\n"
		"}\n";
}

void CppFile::generate_rollbacker(std::ostream &stream){
	stream << "void " << get_rollbacker_function_name() << "(std::uint32_t type, void *s){\n"
		"switch (type){\n";
	for (auto &kv : this->type_map){
		stream << "case " << kv.first << ":\n";
		kv.second->generate_rollbacker(stream, "s");
		stream << "break;\n";
	}
	stream <<
		"}\n"
		"}\n";
}

void CppFile::generate_is_serializable(std::ostream &stream){
	stream << "bool " << get_is_serializable_function_name() << "(std::uint32_t type){\n"
		"switch (type){\n";
	for (auto &kv : this->type_map){
		stream << "case " << kv.first << ":\n";
		kv.second->generate_is_serializable(stream);
		stream << "break;\n";
	}
	stream <<
		"}\n"
		"return false;\n"
		"}\n";
}

void CppFile::generate_aux(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".aux.generated.cpp").c_str());

	std::string array_name = this->get_name() + "_id_hashes";

	file <<
		"#include \"Serializable.h\"\n"
		"#include \"" << this->name << ".generated.h\"\n"
		"#include <utility>\n"
		"#include <cstdint>\n"
		"\n";
	for (auto &kv : this->type_map)
		file << "// " << kv.first << ": " << kv.second << std::endl;
	file <<
		"\n"
		"std::pair<std::uint32_t, TypeHash> " << array_name << "[] = {\n";

	for (auto &i : this->type_map)
		file << "{ " << i.first << ", TypeHash(" << i.second->get_type_hash().to_string() << ")},\n";

	file <<
		"};\n"
		"size_t " << this->get_name() << "_id_hashes_length = " << this->type_map.size() << ";\n"
		"\n";
	this->generate_allocator(file);
	file << "\n";
	this->generate_constructor(file);
	file << "\n";
	this->generate_rollbacker(file);
	file << "\n";
	this->generate_is_serializable(file);
	file <<
		"\n"
		<< generate_get_metadata_signature() << "{\n"
			"std::shared_ptr<SerializableMetadata> ret(new SerializableMetadata);\n"
			"ret->set_functions("
				<< get_allocator_function_name() << ", "
				<< get_constructor_function_name() << ", "
				<< get_rollbacker_function_name() << ", "
				<< get_is_serializable_function_name()
			<<");\n"
			"for (auto &p : " << array_name << ")\n"
				"ret->add_type(p.first, p.second);\n"
			"return ret;\n"
		"}\n";
}
