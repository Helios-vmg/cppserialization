#include "las.h"
#include <fstream>

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
	stream << "{\n";
	for (auto &el : this->elements)
		stream << "\t" << el << (el->needs_semicolon() ? ";\n" : "\n");
	stream <<
		"public:\n"
		"\t";
	stream <<
		this->name << "(DeserializationStream &);\n"
		"\tObjectNode get_object_node() override;\n"
		"\tvoid serialize(SerializerStream &) const override;\n"
		"\tTypeId get_typeid() const override;\n";
		"};\n";
}

void UserClass::generate_source(std::ostream &stream) const{
	stream << this->name << "::" << this->name << "(DeserializationStream &ds){\n";
	stream <<
		"}\n"
		"\n"
		"ObjectNode " << this->name << "::get_object_node(){\n";
	stream <<
		"}\n"
		"\n"
		"void " << this->name << "::serialize(SerializerStream &ss) const{\n";
	stream <<
		"}\n"
		"\n"
		"TypeId " << this->name << "::get_typeid() const{\n";
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
			file << "class " << Class->get_name() << ";\n";
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
	std::ofstream file((this->get_name() + ".genereated.cpp").c_str());
}
