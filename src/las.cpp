#include "las.h"
#include "util.h"
#include <fstream>
#include <sstream>
#include <array>
#include <boost/format.hpp>

template <typename T>
void generate_for_collection(boost::format &format, const std::string &next_name, const std::initializer_list<T> &list, generate_pointer_enumerator_callback_t &callback){
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
		callback((format % accum).str(), CallMode::AddVerbatim);
}

void ArrayType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	boost::format format("for (size_t %1% = 0; %1% != %2%; %1%++){ %3% }");
	auto index = get_unique_varname();
	format = format % index % this->length;
	std::string next_name = (boost::format("(%1%)[%2%]") % this_name % index).str();
	generate_for_collection(format, next_name, { this->inner }, callback);
}

void PointerType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	callback(this_name, CallMode::TransformAndAdd);
}

void SharedPtrType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	callback((boost::format("(%1%).get()") % this_name).str(), CallMode::TransformAndAdd);
}

void SequenceType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	boost::format format("for (const auto &%1% : (%2%)){ %3% }");
	auto index = get_unique_varname();
	format = format % index % this_name;
	generate_for_collection(format, index, { this->inner }, callback);
}

void AssociativeArrayType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	boost::format format("for (const auto &%1% : (%2%)){ %3% }");
	auto index = get_unique_varname();
	format = format % index % this_name;
	generate_for_collection(format, index, { this->first, this->second }, callback);
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
	stream << "class " << this->name;
	if (this->base_classes.size() > 0){
		stream << " : public Serializable";
		for (auto &base : this->base_classes)
			stream << ", public " << base->name;
	}
	stream << "{ ";
	for (auto &el : this->elements)
		stream << "" << el << (el->needs_semicolon() ? "; " : " ");
	stream <<
		"public: "
		"";
	stream <<
		this->name << "(DeserializationStream &); "
		"ObjectNode get_object_node() override; "
		"void get_object_node(std::vector<ObjectNode> &); "
		"void serialize(SerializerStream &) const override; "
		"TypeId get_type_id() const override; ";
		"}; ";
}

void UserClass::generate_get_object_node2(std::ostream &stream) const{
	for (auto &i : this->base_classes)
		stream << i->get_name() << "::get_object_node(v); ";

	auto callback = [&stream](const std::string &s, CallMode mode){
		std::string addend;
		if (mode != CallMode::AddVerbatim){
			addend = (boost::format("v.push_back(get_object_node(%1%)); ") % s).str();
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

void UserClass::generate_get_object_node(std::ostream &stream) const{
	stream <<
		"return\n"
		"{\n"
			"this,\n"
			"[this](){"
				"auto v = make_shared(new std::vector<ObjectNode>);"
				"this->get_object_node(*v);"
				"if (!v->size())\n"
					"return NodeIterator();"
				"return NodeIterator(v);"
			"},\n"
			"[this](SerializerStream &ss){"
				"ss.serialize_id(this);"
			"},\n"
			"[this](){"
				"return this->get_type_id();"
			"}"
		"};\n";
}

void UserClass::generate_serialize(std::ostream &stream) const{
}

void UserClass::generate_source(std::ostream &stream) const{
	stream << this->name << "::" << this->name << "(DeserializationStream &ds){ ";
	stream <<
		"}\n"
		"\n"
		"void " << this->name << "::get_object_node(std::vector<ObjectNode> &v){ ";
	this->generate_get_object_node2(stream);
	stream <<
		"}\n"
		"\n"
		"ObjectNode " << this->name << "::get_object_node(){ ";
	this->generate_get_object_node(stream);
	stream <<
		"}\n"
		"\n"
		"void " << this->name << "::serialize(SerializerStream &ss) const{ ";
	stream <<
		"}\n"
		"\n"
		"std::uint32_t " << this->name << "::get_type_id() const{ ";
	stream <<"}\n";
}

void CppFile::generate_header(){
	std::ofstream file((this->get_name() + ".generated.h").c_str());
	std::set<std::string> includes;
	for (auto &c : this->classes)
		c.second->add_headers(includes);
	for (auto &h : includes)
		file << "#include " << h << std::endl;
	file << "#include \"serialization_utils.h\"\n\n";

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (Class)
			file << "class " << Class->get_name() << "; ";
	}

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (Class)
			Class->generate_header(file);
		else
			file << e;
		file << std::endl;
	}
}

void CppFile::generate_source(){
	std::ofstream file((this->get_name() + ".generated.cpp").c_str());
	file << "#include \"" << this->get_name() << ".generated.h\"\n"
		"\n";

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (!Class)
			continue;
		Class->generate_source(file);
		file << std::endl;
	}
}
