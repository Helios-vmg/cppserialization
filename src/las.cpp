#include "las.h"
#include "util.h"
#include <fstream>
#include <sstream>
#include <array>
#include <boost/format.hpp>
#include "variable_formatter.h"
#include "typehash.h"

std::string IntegerType::get_type_string() const{
	std::stringstream stream;
	stream << (this->signedness ? 's' : 'u') << (8 << this->size);
	return stream.str();
}

std::string ArrayType::get_type_string() const{
	std::stringstream stream;
	stream << this->inner->get_type_string() << '[' << this->length << ']';
	return stream.str();
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
	callback(this_name, CallMode::TransformAndAdd);
}

void SharedPtrType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	callback((boost::format("(%1%).get()") % this_name).str(), CallMode::TransformAndAdd);
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
		"virtual const std::pair<std::uint32_t, TypeHash> *get_type_hashes_list(size_t &length) const override;\n"
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

void UserClass::generate_source(std::ostream &stream) const{
	static const char *format =
		"{name}::{name}(DeserializerStream &ds){{\n"
		"{ctor}"
		"}}\n"
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
		"const std::pair<std::uint32_t, TypeHash> *{name}::get_type_hashes_list(size_t &length) const{{\n"
		"    return ::get_type_hashes_list(length);"
		"}}\n";
	variable_formatter vf = format;
	vf
		<< "name" << this->name
		<< "ctor" << ""
		<< "gon" << this->generate_get_object_node2()
		<< "ser" << this->generate_serialize()
		<< "gti" << ("return " + utoa(this->get_type_id()) + ";")
		<< "gth" << this->generate_get_type_hash();
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

	auto &type_map = this->type_map;
	auto callback = [&type_map, &id](Type &t, std::uint32_t &type_id){
		auto &hash = t.get_type_hash();
		auto it = type_map.left.find(hash);
		if (it != type_map.left.end())
			return;
		type_id = id;
		type_map.insert(type_map_t::value_type(hash, id));
		id++;
	};

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (!Class)
			continue;
		Class->iterate(callback);
	}
}

void CppFile::generate_header(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".generated.h").c_str());
	std::set<std::string> includes;
	for (auto &c : this->classes)
		c.second->add_headers(includes);
	for (auto &h : includes)
		file << "#include " << h << std::endl;
	file <<
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

void CppFile::generate_source(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".generated.cpp").c_str());
	file << "#include \"" << this->get_name() << ".generated.h\"\n"
		"#include <utility>\n"
		"\n"
		"extern std::pair<std::uint32_t, TypeHash> " << this->get_name() << "_id_hashes[];\n"
		"extern size_t " << this->get_name() << "_id_hashes_length;\n"
		"\n"
		"static std::pair<std::uint32_t, TypeHash> *get_type_hashes_list(size_t &l){\n"
			"l = " << this->get_name() << "_id_hashes_length;\n"
			"return " << this->get_name() << "_id_hashes;\n"
		"}\n"
		"\n";

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (!Class)
			continue;
		Class->generate_source(file);
		file << std::endl;
	}
}

void CppFile::generate_aux(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".aux.generated.cpp").c_str());

	file <<
		"#include \"Serializable.h\"\n"
		"#include <utility>\n"
		"#include <cstdint>\n"
		"\n"
		"std::pair<std::uint32_t, TypeHash> " << this->get_name() << "_id_hashes[] = {\n";

	for (auto &i : this->type_map.left)
		file << "{ " << i.second << ", TypeHash(" << i.first.to_string() << ")},\n";

	file << "};\n"
		"size_t " << this->get_name() << "_id_hashes_length = " << this->type_map.left.size() << ";\n";
}
