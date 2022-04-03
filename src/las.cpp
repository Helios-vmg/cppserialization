#include "stdafx.h"
#include "las.h"
#include "util.h"
#include "variable_formatter.h"
#include "typehash.h"
#include "GenericException.h"
#ifndef HAVE_PRECOMPILED_HEADERS
#include <fstream>
#include <sstream>
#include <array>
#include <iomanip>
#include <cctype>
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
DEFINE_get_X_function_name(dynamic_cast)
DEFINE_get_X_function_name(allocate_pointer)
DEFINE_get_X_function_name(categorize_cast)

std::string IntegerType::get_type_string() const{
	std::stringstream stream;
	stream << (this->signedness ? 's' : 'u') << (8 << this->size);
	return stream.str();
}

std::string FloatingPointType::get_type_string() const{
	const char *s;
	switch (this->precision) {
		case FloatingPointPrecision::Float:
			s = "float";
			break;
		case FloatingPointPrecision::Double:
			s = "double";
			break;
		default:
			break;
	}
	return s;
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
		"for (size_t i = 0; i != " << this->length << "; i++){\n";
	this->generate_deserializer(stream, deserializer_name, pointer_name);
	stream << "}\n"
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
	stream << "{}";
}

void Type::generate_is_serializable(std::ostream &stream) const{
	stream << "return " << (this->get_is_serializable() ? "true" : "false") << ";\n";
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
	if (!this->inner->is_serializable())
		stream << "::get_object_node(" << this_name << ", static_get_type_id<" << this->inner << ">::value)";
	else
		stream << "::get_object_node((Serializable *)" << this_name << ")";
	callback(stream.str(), CallMode::TransformAndAdd);
}

void StdSmartPtrType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	std::stringstream stream;
	if (!this->inner->is_serializable())
		stream << "::get_object_node((" << this_name << ").get(), static_get_type_id<" << this->inner << ">::value)";
	else
		stream << "::get_object_node((Serializable *)(" << this_name << ").get())";
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
		bc.Class->add_headers(set);
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
			stream << "public ";
			if (base.Virtual)
				stream << "virtual ";
			stream << base.Class->name;
		}
	}else{
		stream << "public ";
		if (this->inherit_Serializable_virtually)
			stream << "virtual ";
		stream << "Serializable";
	}
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
		stream << i.Class->get_name() << "::get_object_node(v);\n";

	auto callback = [&stream](const std::string &s, CallMode mode){
		std::string addend;
		if (mode != CallMode::AddVerbatim){
			auto replaced = replace_all(s, "(*this).", "this->");
			addend = variable_formatter("v.push_back({0});\n") << "0" << replaced;
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
		stream << b.Class->get_name() << "::serialize(ss);\n";
	
	for (auto &e : this->elements){
		auto casted = std::dynamic_pointer_cast<ClassMember>(e);
		if (!casted)
			continue;
		stream << "ss.serialize(this->" << casted->get_name() << ");\n";
	}
}

void UserClass::generate_get_type_hash(std::ostream &stream) const{
	stream << "return " << this->root->get_name() << "_id_hashes[" << (this->get_type_id() - 1) << "].second;";
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
		stream << b.Class->get_name() << "(ds)\n";
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

bool UserClass::get_is_serializable() const{
	return true;
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
	variable_formatter vf(format);
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
		b.Class->iterate(callback, visited);
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
		b.Class->iterate_only_public(callback, visited, false);
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
		stream << ':' << b.Class->base_get_type_string();
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

void UserClass::walk_class_hierarchy(const std::function<bool(UserClass &)> &callback){
	if (!callback(*this))
		return;
	for (auto &p : this->base_classes)
		p.Class->walk_class_hierarchy(callback);
}

void UserClass::mark_virtual_inheritances(std::uint32_t serializable_type_id){
	if (this->base_classes.size() < 2)
		return;

	std::vector<unsigned> counts;
	for (auto &bc : this->base_classes){
		auto &abs = bc.Class->get_all_base_classes();
		for (auto c : abs){
			auto id = !c ? serializable_type_id : c->get_type_id();
			if (counts.size() <= id)
				counts.resize(id + 1);
			counts[id]++;
		}
	}
	std::vector<std::uint32_t> virtual_classes;
	for (size_t i = 0; i < counts.size(); i++)
		if (counts[i] > 1)
			virtual_classes.push_back(i);
	this->mark_virtual_inheritances(serializable_type_id, virtual_classes);
}

void UserClass::mark_virtual_inheritances(std::uint32_t serializable_type_id, const std::vector<std::uint32_t> &virtual_classes){
	auto b = virtual_classes.begin();
	auto e = virtual_classes.end();
	if (!this->base_classes.size()){
		auto it = std::lower_bound(b, e, serializable_type_id);
		if (it == e || *it != serializable_type_id)
			return;
		this->inherit_Serializable_virtually = true;
		return;
	}

	for (auto &bc : this->base_classes){
		bc.Class->mark_virtual_inheritances(serializable_type_id, virtual_classes);
		auto it = std::lower_bound(b, e, bc.Class->get_type_id());
		if (it == e || *it != bc.Class->get_type_id())
			continue;
		bc.Virtual = true;
	}
}

bool UserClass::is_subclass_of(const UserClass &other) const{
	if (this->get_type_id() == other.get_type_id())
		return true;
	for (auto &c : this->base_classes)
		if (c.Class->is_subclass_of(other))
			return true;
	return false;
}

bool less_than(UserClass *a, UserClass *b){
	if (!b)
		return false;
	if (!a)
		return true;
	return a->get_type_id() < b->get_type_id();
}

const std::vector<UserClass *> &UserClass::get_all_base_classes(){
	auto &tabs = this->all_base_classes;
	if (!tabs.size()){
		tabs.push_back(this);
		if (!this->base_classes.size())
			tabs.push_back(nullptr);
		for (auto &bs : this->base_classes){
			auto &abs = bs.Class->get_all_base_classes();
			decltype(this->all_base_classes) temp(this->all_base_classes.size() + abs.size());
			auto it = std::set_union(tabs.begin(), tabs.end(), abs.begin(), abs.end(), temp.begin(), less_than);
			temp.resize(it - temp.begin());
			tabs = std::move(temp);
		}
	}
	return this->all_base_classes;
}

bool UserClass::is_trivial_class(){
	if (this->trivial_class < 0){
		if (!this->base_classes.size())
			this->trivial_class = 1;
		else if (this->base_classes.size() > 1 || this->base_classes.front().Virtual)
			this->trivial_class = 0;
		else
			this->trivial_class = this->base_classes.front().Class->is_trivial_class();
	}
	return !!this->trivial_class;
}

CppVersion UserClass::minimum_cpp_version() const{
	auto ret = CppVersion::Cpp1998;
	for (auto &base : this->base_classes){
		auto v = base.Class->minimum_cpp_version();
		if ((long)v > (long)ret)
			ret = v;
	}
	for (auto &element : this->elements){
		auto v = element->minimum_cpp_version();
		if ((long)v > (long)ret)
			ret = v;
	}
	return ret;
}

std::uint32_t CppFile::assign_type_ids(){
	std::uint32_t id = 1;
	
	if (!this->type_map.size()){
		std::map<TypeHash, std::pair<unsigned, Type *>> type_map;
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
	}else{
		for (auto &kv : this->type_map)
			id = std::max(id, kv.first);
		id++;
	}

	return id;
}

std::string filename_to_macro(std::string ret){
	for (auto &c : ret)
		if (c == '.')
			c = '_';
		else
			c = toupper(c);
	return ret;
}

CppVersion CppFile::minimum_cpp_version() const{
	CppVersion ret = CppVersion::Cpp1998;
	for (auto &c : this->classes){
		auto v = c.second->minimum_cpp_version();
		if ((long)v > (long)ret)
			ret = v;
	}
	return ret;
}

const char *to_string(CppVersion version){
	switch (version){
		case CppVersion::Undefined:
			return "C++??";
		case CppVersion::Cpp1998:
			return "C++98";
		case CppVersion::Cpp2011:
			return "C++11";
		case CppVersion::Cpp2014:
			return "C++14";
		case CppVersion::Cpp2017:
			return "C++17";
		case CppVersion::Cpp2020:
			return "C++20";
		default:
			throw GenericException("Invalid switch for CppVersion");
	}
}

void require_cpp(std::ostream &s, CppVersion version){
	s <<
		"#ifndef __cplusplus\n"
		"#error No C++?\n"
		"#endif\n"
		"\n";
	if (version == CppVersion::Undefined)
		return;
	s <<
		"#if __cplusplus < " << (int)version << "\n"
		"#error " << to_string(version) << " or newer is required to compile this.\n"
		"#endif\n"
		"\n";
}

void CppFile::generate_header(){
	this->assign_type_ids();

	auto filename = this->get_name() + ".generated.h";
	auto macro = filename_to_macro(filename);

	std::ofstream file(filename.c_str());

	file <<
		"#pragma once\n"
		"\n"
		;
	require_cpp(file, this->minimum_cpp_version());

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
		"#include \"serialization_utils.inl\"\n"
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

	file << "\n";
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
		"\n"
		<< generate_get_metadata_signature() << ";\n"
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
		"static const size_t sizes[] = { ";
	for (auto &kv : this->type_map){
		if (kv.second->is_abstract()){
			stream << "0, ";
			continue;
		}
		stream << "sizeof(";
		kv.second->output(stream);
		stream << "), ";
	}
	stream << "};\n"
		"type--;\n"
		"if (!sizes[type]) return nullptr;\n"
		"return ::operator new (sizes[type]);\n"
		"}\n";
}

void CppFile::generate_constructor(std::ostream &stream){
	stream << "void " << get_constructor_function_name() << "(std::uint32_t type, void *s, DeserializerStream &ds){\n"
		"typedef void (*constructor_f)(void *, DeserializerStream &);\n"
		"static const constructor_f constructors[] = {\n";
	for (auto &kv : this->type_map){
		if (kv.second->is_abstract()){
			stream << "nullptr,\n";
			continue;
		}
		stream << "[](void *s, DeserializerStream &ds)";
		kv.second->generate_deserializer(stream, "ds", "s");
		stream << ",\n";
	}
	stream << "};\n"
		"type--;\n"
		"if (!constructors[type]) return;\n"
		"return constructors[type](s, ds);\n"
		"}\n";
}

void CppFile::generate_rollbacker(std::ostream &stream){
	stream << "void " << get_rollbacker_function_name() << "(std::uint32_t type, void *s){\n"
		"typedef void (*rollbacker_f)(void *);\n"
		"static const rollbacker_f rollbackers[] = {\n";
	for (auto &kv : this->type_map) {
		if (kv.second->is_abstract()) {
			stream << "nullptr,\n";
			continue;
		}
		stream << "[](void *s)";
		kv.second->generate_rollbacker(stream, "s");
		stream << ",\n";
	}
	stream << "};\n"
		"type--;\n"
		"if (!rollbackers[type]) return;\n"
		"return rollbackers[type](s);\n"
		"}\n";
}

void CppFile::generate_is_serializable(std::ostream &stream){
	std::vector<std::uint32_t> flags;
	int bit = 0;
	int i = 0;
	for (auto &kv : this->type_map){
		std::uint32_t u = kv.second->get_is_serializable();
		if (kv.second->get_type_id() != ++i)
			throw GenericException("Unknown program state!");
		u <<= bit;
		if (!bit)
			flags.push_back(u);
		else
			flags.back() |= u;
		bit = (bit + 1) % 32;
	}


	const char *format = 
		"bool {function_name}(std::uint32_t type){{\n"
		"static const std::uint32_t is_serializable[] = {{ {array} }};\n"
		"type--;\n"
		"return (is_serializable[type / 32] >> (type & 31)) & 1;\n"
		"}}\n";
	variable_formatter vf(format);
	std::stringstream temp;
	for (auto u : flags)
		temp << "0x" << std::hex << std::setw(8) << std::setfill('0') << (int)u << ", ";
	vf
		<< "function_name" << get_is_serializable_function_name()
		<< "array" << temp.str();
	
	stream << (std::string)vf;
}

#if 0
void CppFile::generate_cast_offsets(std::ostream &stream){
	std::vector<std::pair<std::uint32_t, std::uint32_t>> valid_casts;
	for (auto &kv1 : this->type_map){
		if (!kv1.second->is_serializable())
			continue;
		auto uc1 = static_cast<UserClass *>(kv1.second);
		for (auto &kv2 : this->type_map){
			if (!kv2.second->is_serializable())
				continue;
			auto uc2 = static_cast<UserClass *>(kv2.second);
			if (uc1->is_subclass_of(*uc2))
				valid_casts.push_back(std::make_pair(kv1.first, kv2.first));
		}
	}
	std::sort(valid_casts.begin(), valid_casts.end());


	const char *format =
		"std::vector<std::tuple<std::uint32_t, std::uint32_t, int>> {function_name}(){{\n"
		"const auto k = std::numeric_limits<intptr_t>::max() / 4;\n"
		"std::vector<std::tuple<std::uint32_t, std::uint32_t, int>> ret;\n"
		"ret.reserve({size});\n"
		"{ret_init}"
		"return ret;\n"
		"}}\n";
	variable_formatter vf(format);
	std::stringstream temp;
	for (auto &kv : valid_casts){
		temp <<
			"ret.push_back("
			"std::make_tuple("
			"(std::uint32_t)" << kv.first << ", "
			"(std::uint32_t)" << kv.second << ", ";
		if (kv.first != kv.second){
			temp << "(int)((intptr_t)static_cast<";
			this->type_map[kv.second]->output(temp);
			temp << " *>((";
			this->type_map[kv.first]->output(temp);
			temp << " *)k) - k)";
		}else
			temp << "0";
		temp << "));\n";
	}

	vf
		<< "function_name" << get_cast_offsets_function_name()
		<< "size" << valid_casts.size()
		<< "ret_init" << temp.str();

	stream << (std::string)vf;
}
#endif

void CppFile::generate_dynamic_cast(std::ostream &stream){
	stream << "Serializable *" << get_dynamic_cast_function_name() << "(void *src, std::uint32_t type){\n"
		"    typedef Serializable *(*cast_f)(void *);\n"
		"    static const cast_f casts[] = {\n";
	for (auto &kv : this->type_map){
		if (!kv.second->is_serializable()){
			stream << "        nullptr,\n";
			continue;
		}
		stream << "        [](void *p){ return dynamic_cast<Serializable *>((";
		kv.second->output(stream);
		stream << " *)p); },\n";
	}
	stream <<
		"    };\n"
		"    type--;\n"
		"    if (!casts[type])\n"
		"        return nullptr;\n"
		"    return casts[type](src);\n"
		"}\n";
}

void CppFile::generate_generic_pointer_classes(std::ostream &stream){
	for (auto &kv1 : this->type_map){
		if (!kv1.second->is_serializable())
			continue;
		auto uc1 = static_cast<UserClass *>(kv1.second);
		auto name = uc1->get_name();

		stream <<
			"class RawPointer" << name << " : public GenericPointer{\n"
			"    std::unique_ptr<" << name << " *> real_pointer;\n"
			"public:\n"
			"    RawPointer" << name << "(void *p){\n"
			"        this->real_pointer.reset(new " << name << "*((" << name << " *)p));\n"
			"        this->pointer = this->real_pointer.get();\n"
			"    }\n"
			"    ~RawPointer" << name << "() = default;\n"
			"    void release() override{}\n"
			"    std::unique_ptr<GenericPointer> cast(std::uint32_t type) override;\n"
			"};\n"
			"\n"
			;

		stream <<
			"class SharedPtr" << name << " : public GenericPointer{\n"
			"    std::unique_ptr<std::shared_ptr<" << name << ">> real_pointer;\n"
			"public:\n"
			"    SharedPtr" << name << "(void *p){\n"
			"        this->real_pointer.reset(new std::shared_ptr<" << name << ">((" << name << " *)p, conditional_deleter<" << name << ">()));\n"
			"        this->pointer = this->real_pointer.get();\n"
			"    }\n"
			"    template <typename T>\n"
			"    SharedPtr" << name << "(const std::shared_ptr<T> &p){\n"
			"        this->real_pointer.reset(new std::shared_ptr<" << name <<">(std::static_pointer_cast<" << name << ">(p)));\n"
			"        this->pointer = this->real_pointer.get();\n"
			"    }\n"
			"    ~SharedPtr" << name << "() = default;\n"
			"    void release() override{\n"
			"        std::get_deleter<conditional_deleter<" << name << ">> (*this->real_pointer)->disable();\n"
			"    }\n"
			"    std::unique_ptr<GenericPointer> cast(std::uint32_t type) override;\n"
			"};\n"
			"\n"
			;

		stream <<
			"class UniquePtr" << name << " : public GenericPointer{\n"
			"    std::unique_ptr<std::unique_ptr<" << name << ">> real_pointer;\n"
			"public:\n"
			"    UniquePtr" << name << "(void *p){\n"
			"        this->real_pointer.reset(new std::unique_ptr<" << name << ">((" << name << " *)p));\n"
			"        this->pointer = this->real_pointer.get();\n"
			"    }\n"
			"    ~UniquePtr" << name << "() = default;\n"
			"    void release() override{\n"
			"        this->real_pointer->release();\n"
			"    }\n"
			"    std::unique_ptr<GenericPointer> cast(std::uint32_t type) override;\n"
			"};\n"
			"\n"
			;
	}
}

void CppFile::generate_generic_pointer_class_implementations(std::ostream &stream){
	for (auto &kv1 : this->type_map){
		if (!kv1.second->is_serializable())
			continue;
		auto uc1 = static_cast<UserClass *>(kv1.second);
		auto name = uc1->get_name();

		//RawPointer

		stream <<
			"std::unique_ptr<GenericPointer> RawPointer" << name << "::cast(std::uint32_t type){\n"
			"    switch (type){\n";
		for (auto &kv2 : this->type_map){
			if (!kv2.second->is_serializable())
				continue;
			auto uc2 = static_cast<UserClass *>(kv2.second);
			if (!uc1->is_subclass_of(*uc2))
				continue;
			stream <<
				"        case " << uc2->get_type_id() << ":\n"
				"            return std::unique_ptr<GenericPointer>(new RawPointer" << uc2->get_name() << "(*this->real_pointer));\n";
		}
		stream <<
			"        default:\n"
			"            return std::unique_ptr<GenericPointer>();\n"
			"    }\n"
			"}\n";
		
		//SharedPtr

		stream <<
			"std::unique_ptr<GenericPointer> SharedPtr" << name << "::cast(std::uint32_t type){\n"
			"    switch (type){\n";
		for (auto &kv2 : this->type_map){
			if (!kv2.second->is_serializable())
				continue;
			auto uc2 = static_cast<UserClass *>(kv2.second);
			if (!uc1->is_subclass_of(*uc2))
				continue;
			stream <<
				"        case " << uc2->get_type_id() << ":\n"
				"            return std::unique_ptr<GenericPointer>(new SharedPtr" << uc2->get_name() << "(*this->real_pointer));\n";
		}
		stream <<
			"        default:\n"
			"            return std::unique_ptr<GenericPointer>();\n"
			"    }\n"
			"}\n";
		
		//UniquePtr

		stream <<
			"std::unique_ptr<GenericPointer> UniquePtr" << name << "::cast(std::uint32_t type){\n"
			"    switch (type){\n";
		for (auto &kv2 : this->type_map){
			if (!kv2.second->is_serializable())
				continue;
			auto uc2 = static_cast<UserClass *>(kv2.second);
			if (!uc1->is_subclass_of(*uc2))
				continue;
			stream <<
				"        case " << uc2->get_type_id() << ":\n"
				"            throw std::runtime_error(\"Multiple unique pointers to single object detected.\");\n";
		}
		stream <<
			"        default:\n"
			"            return std::unique_ptr<GenericPointer>();\n"
			"    }\n"
			"}\n";

	}
}

void CppFile::generate_pointer_allocator(std::ostream &stream){
	stream <<
		"std::unique_ptr<GenericPointer> " << get_allocate_pointer_function_name() << "(std::uint32_t type, PointerType pointer_type, void *p){\n"
		"    switch (pointer_type){\n"
		"        case PointerType::RawPointer:\n"
		"            switch (type){\n"
		;
	for (auto &kv : this->type_map){
		if (!kv.second->is_serializable())
			continue;
		auto uc = static_cast<UserClass *>(kv.second);
		auto name = uc->get_name();
		stream <<
			"                case " << uc->get_type_id() << ":\n"
			"                    return std::unique_ptr<GenericPointer>(new RawPointer" << name << "(p));\n";
	}
	stream <<
		"                default:\n"
		"                    return std::unique_ptr<GenericPointer>();\n"
		"            }\n"
		"            break;\n"
		"        case PointerType::SharedPtr:\n"
		"            switch (type){\n"
		;
	for (auto &kv : this->type_map){
		if (!kv.second->is_serializable())
			continue;
		auto uc = static_cast<UserClass *>(kv.second);
		auto name = uc->get_name();
		stream <<
			"                case " << uc->get_type_id() << ":\n"
			"                    return std::unique_ptr<GenericPointer>(new SharedPtr" << name << "(p));\n";
	}
	stream <<
		"                default:\n"
		"                    return std::unique_ptr<GenericPointer>();\n"
		"            }\n"
		"            break;\n"
		"        case PointerType::UniquePtr:\n"
		"            switch (type){\n"
		;
	for (auto &kv : this->type_map){
		if (!kv.second->is_serializable())
			continue;
		auto uc = static_cast<UserClass *>(kv.second);
		auto name = uc->get_name();
		stream <<
			"                case " << uc->get_type_id() << ":\n"
			"                    return std::unique_ptr<GenericPointer>(new UniquePtr" << name << "(p));\n";
	}
	stream <<
		"                default:\n"
		"                    return std::unique_ptr<GenericPointer>();\n"
		"            }\n"
		"            break;\n"
		"        default:\n"
		"            throw std::runtime_error(\"Internal error. Unknown pointer type.\");\n"
		"    }\n"
		"}\n"
		;
}

void CppFile::generate_cast_categorizer(std::ostream &stream){
	unsigned max_type = 0;
	for (auto &kv : this->type_map)
		max_type = std::max(max_type, kv.first);

	stream <<
		"CastCategory " << get_categorize_cast_function_name() << "(std::uint32_t src_type, std::uint32_t dst_type){\n"
		"    if (src_type < 1 || dst_type < 1 || src_type > " << max_type << " || dst_type > " << max_type << ")\n"
		"        return CastCategory::Invalid;\n"
		"    if (src_type == dst_type)\n"
		"        return CastCategory::Trivial;\n"
		"    static const CastCategory categories[] = {\n"
		;
	for (unsigned src = 1; src <= max_type; src++){
		for (unsigned dst = 1; dst <= max_type; dst++){
			if (src == dst){
				stream << "        CastCategory::Trivial,\n";
				continue;
			}
			auto src_t = this->type_map[src];
			auto dst_t = this->type_map[dst];
			if (!src_t->is_serializable() || !dst_t->is_serializable()){
				stream << "        CastCategory::Invalid,\n";
				continue;
			}
			auto src_class = static_cast<UserClass *>(src_t);
			auto dst_class = static_cast<UserClass *>(dst_t);
			if (!src_class->is_subclass_of(*dst_class)){
				stream << "        CastCategory::Invalid,\n";
				continue;
			}
			if (src_class->is_trivial_class()){
				stream << "        CastCategory::Trivial,\n";
				continue;
			}
			stream << "        CastCategory::Complex,\n";
		}
	}
	stream <<
		"    };\n"
		"    src_type--;\n"
		"    dst_type--;\n"
		"    return categories[dst_type + src_type * " << max_type << "];\n"
		"}\n"
		;
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
		"#include <memory>\n"
		"\n";
	for (auto &kv : this->type_map)
		file << "// " << kv.first << ": " << kv.second << std::endl;
	file <<
		"\n"
		"std::pair<std::uint32_t, TypeHash> " << array_name << "[] = {\n";

	for (auto &i : this->type_map){
		auto type = i.second;
		auto ts = type->get_type_string();
		if (dynamic_cast<UserClass *>(type))
			ts = static_cast<UserClass *>(type)->base_get_type_string();
		file
			<< "// " << ts << "\n"
			<< "{ " << i.first << ", TypeHash(" << type->get_type_hash().to_string() << ")},\n";
	}

	file <<
		"};\n"
		"\n";
	this->generate_allocator(file);
	file << "\n";
	this->generate_constructor(file);
	file << "\n";
	this->generate_rollbacker(file);
	file << "\n";
	this->generate_is_serializable(file);
	file << "\n";
	this->generate_dynamic_cast(file);
	file << "\n";
	this->generate_generic_pointer_classes(file);
	file << "\n";
	this->generate_generic_pointer_class_implementations(file);
	file << "\n";
	this->generate_pointer_allocator(file);
	file << "\n";
	this->generate_cast_categorizer(file);
	file <<
		"\n"
		<< generate_get_metadata_signature() << "{\n"
			"std::shared_ptr<SerializableMetadata> ret(new SerializableMetadata);\n"
			"ret->set_functions("
				<< get_allocator_function_name() << ", "
				<< get_constructor_function_name() << ", "
				<< get_rollbacker_function_name() << ", "
				<< get_is_serializable_function_name() << ", "
				<< get_dynamic_cast_function_name() << ", "
				<< get_allocate_pointer_function_name() << ", "
				<< get_categorize_cast_function_name()
			<<");\n"
			"for (auto &p : " << array_name << ")\n"
				"ret->add_type(p.first, p.second);\n"
			"return ret;\n"
		"}\n";
}

void CppFile::mark_virtual_inheritances(){
	auto serializable_type_id = this->assign_type_ids();
	for (auto &uc : this->classes)
		uc.second->mark_virtual_inheritances(serializable_type_id);
}
