#include "las.h"
#include "tinyxml2/tinyxml2.h"
#include "util.h"
#include <unordered_map>
#include <sstream>
#include <functional>
#include <fstream>

template <typename T>
std::shared_ptr<T> make_shared(T *p){
	return std::shared_ptr<T>(p);
}

using tinyxml2::XMLDocument;
using tinyxml2::XMLElement;

typedef Type *(*type_creator_t)(const XMLElement *);

std::unordered_map<std::string, std::function<std::shared_ptr<Type> (const XMLElement *)> > callback_map;

std::shared_ptr<Type> create_bool_type(const XMLElement *){
	return make_shared(new BoolType);
}

std::shared_ptr<Type>create_int_type(bool signedness, unsigned size){
	return make_shared(new IntegerType(signedness, size));
}

std::shared_ptr<Type> create_string_type(bool wide){
	return make_shared(new StringType(wide));
}

std::shared_ptr<Type> create_shared_ptr_t(const XMLElement *parent){
	for (auto el = parent->FirstChildElement(); el; el = el->NextSiblingElement()){
		auto it = callback_map.find((std::string)el->Name());
		if (it == callback_map.end())
			continue;
		return make_shared(new SharedPtrType(it->second(el)));
	}
	return nullptr;
}

std::shared_ptr<Type> create_array_type(const XMLElement *parent){
	auto length = parent->IntAttribute("length");
	if (!length)
		return nullptr;
	for (auto el = parent->FirstChildElement(); el; el = el->NextSiblingElement()){
		auto it = callback_map.find((std::string)el->Name());
		if (it == callback_map.end())
			continue;
		return make_shared(new ArrayType(it->second(el), (size_t)length));
	}
	return nullptr;
}

std::shared_ptr<Type> create_pointer_type(const XMLElement *parent){
	for (auto el = parent->FirstChildElement(); el; el = el->NextSiblingElement()){
		auto it = callback_map.find((std::string)el->Name());
		if (it == callback_map.end())
			continue;
		return make_shared(new PointerType(it->second(el)));
	}
	return nullptr;
}

struct init_maps{
	init_maps(){
		callback_map["bool"] = create_bool_type;
		for (int sig = 0; sig < 2; sig++){
			for (int siz = 0; siz < 4; siz++){
				auto name = (std::string)((sig) ? "" : "u") + "int" + utoa(8 << siz) + "_t";
				callback_map[name] = [sig, siz](const XMLElement *){ return create_int_type(!!sig, siz); };
			}
		}
		callback_map["string"] = [](const XMLElement *){ return create_string_type(false); };
		callback_map["wstring"] = [](const XMLElement *){ return create_string_type(true); };
		callback_map["shared_ptr"] = create_shared_ptr_t;
		callback_map["array"] = create_array_type;
		callback_map["pointer"] = create_pointer_type;
	}
} init_maps_instance;

void iterate_class(
		std::shared_ptr<UserClass> Class,
		const std::unordered_map<std::string, std::shared_ptr<UserClass> > &classes,
		const XMLElement *parent,
		Accessibility default_accessibility){
	for (auto el = parent->FirstChildElement(); el; el = el->NextSiblingElement()){
		auto name = (std::string)el->Name();
		if (name == "public")
			default_accessibility = Accessibility::Public;
		else if (name == "protected")
			default_accessibility = Accessibility::Protected;
		else if (name == "private")
			default_accessibility = Accessibility::Private;
		else if (name == "include"){
			auto header = el->Attribute("header");
			if (!header)
				continue;
			Class->add_element(make_shared(new UserInclude(header)));
		}else if (name == "base"){
			auto base = el->Attribute("base");
			if (!base)
				continue;
			auto it = classes.find((std::string)base);
			if (it == classes.end())
				continue;
			Class->add_base_class(it->second);
		}else{
			auto attr_name = el->Attribute("name");
			if (!attr_name)
				continue;

			auto it = callback_map.find(name);
			std::shared_ptr<Type> type;
			if (it == callback_map.end()){
				auto j = classes.find(name);
				if (j == classes.end()){
					if (name != Class->get_name()) 
						continue;
					type = Class;
				}else
					type = j->second;
			}else
				type = it->second(el);
			auto member = make_shared(new ClassMember(type, attr_name, default_accessibility));
			Class->add_element(member);
		}
	}
}

void iterate_cpp(CppFile &cpp, const XMLElement *parent){
	for (auto el = parent->FirstChildElement(); el; el = el->NextSiblingElement()){
		auto name = el->Name();
		std::shared_ptr<CppElement> cpp_element;
		if (!strcmp(name, "include")){
			auto header = el->Attribute("header");
			if (!header)
				continue;

			cpp_element.reset(new UserInclude(header));
		}else if (!strcmp(name, "struct")){
			name = el->Attribute("name");
			if (!name)
				continue;

			auto Class = make_shared(new UserClass(name));
			callback_map[name] = [Class](const XMLElement *){ return Class; };
			cpp_element = Class;
			iterate_class(Class, cpp.get_classes(), el, Accessibility::Public);
		}else if (!strcmp(name, "class")){
			name = el->Attribute("name");
			if (!name)
				continue;

			auto Class = make_shared(new UserClass(name));
			callback_map[name] = [Class](const XMLElement *){ return Class; };
			cpp_element = Class;
			iterate_class(Class, cpp.get_classes(), el, Accessibility::Private);
		}
		if (cpp_element)
			cpp.add_element(cpp_element);
	}
}

std::vector<std::shared_ptr<CppFile> > iterate_document(const XMLDocument &doc){
	std::vector<std::shared_ptr<CppFile> > ret;
	for (auto el = doc.FirstChildElement(); el; el = el->NextSiblingElement()){
		if (strcmp(el->Name(), "cpp"))
			continue;

		auto name = el->Attribute("name");
		if (!name)
			continue;

		std::shared_ptr<CppFile> cpp(new CppFile(name));
		ret.push_back(cpp);

		iterate_cpp(*cpp, el);
	}
	return ret;
}

int main(int argc, char **argv){
	if (argc < 2)
		return -1;
	XMLDocument doc;
	doc.LoadFile(argv[1]);
	auto cpps = iterate_document(doc);
	for (auto &cpp : cpps){
		cpp->generate_header();
		cpp->generate_source();
		cpp->generate_aux();
	}
}
