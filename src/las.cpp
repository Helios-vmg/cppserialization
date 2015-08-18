#include "las.h"
#include <fstream>

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
		stream << " : ";
		bool first = true;
		for (auto &base : this->base_classes){
			if (!first)
				stream << ", ";
			else
				first = false;
			stream << "public " << base->name;
		}
	}
	stream << "{\n";
	for (auto &el : this->elements)
		stream << "\t" << el << (el->needs_semicolon() ? ";\n" : "\n");
	stream << "\tpublic: " << this->name << "(std::istream &);\n"
		"\tvoid serialize(SerializationStream &);\n"
		"\tNodeIterator get_node_iterator();\n";
	stream << "};\n";
}

void UserClass::generate_source(std::ostream &stream) const{
	stream << "NodeIterator " << this->name << "::get_node_iterator(){\n";

	stream <<"}\n";
	stream << "void " << this->name << "::serialize(SerializationStream &ss){\n";

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
