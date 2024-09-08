#include "las.hpp"
#include "util.hpp"
#include "variable_formatter.hpp"
#include "typehash.hpp"
#include "nonterminal.hpp"
#include <fstream>
#include <sstream>
#include <array>
#include <iomanip>
#include <cctype>

using namespace std::string_literals;

std::mt19937_64 rng;
std::uint64_t random_function_name = 0;

void get_randomized_function_name(std::string &name, const char *custom_part){
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
DEFINE_get_X_function_name(check_enum)

std::string IntegerType::get_source_name() const{
	return "std::"s + (!this->signedness ? "u" : "") + "int" + std::to_string(8 << this->size) + "_t";
}

std::string IntegerType::get_serializer_name() const{
	std::stringstream stream;
	stream << (this->signedness ? 's' : 'u') << (8 << this->size);
	return stream.str();
}

bool IntegerType::check_value(const EasySignedBigNum &value){

	static const std::pair<const char *, const char *> limits[] = {
		{                   "0",                  "256"},
		{                "-128",                  "128"},
		{                   "0",                "65536"},
		{              "-32768",                "32768"},
		{                   "0",           "4294967296"},
		{         "-2147483648",           "2147483648"},
		{                   "0", "18446744073709551616"},
		{"-9223372036854775808",  "9223372036854775808"},
	};

	auto index = this->size * 2 + (int)this->signedness;
	if (index < 0 || index > sizeof(limits) / sizeof(*limits))
		return false;
	auto [begin, end] = limits[index];
	return EasySignedBigNum(begin) <= value && value < EasySignedBigNum(end);
}

std::string FloatingPointType::get_serializer_name() const{
	switch (this->precision) {
		case FloatingPointPrecision::Float:
			return "float";
		case FloatingPointPrecision::Double:
			return "double";
		default:
			throw std::exception();
	}
}

std::string StringType::get_source_name() const{
	const char *s = "";
	switch (this->width) {
		case CharacterWidth::Narrow:
			s = "string";
			break;
		case CharacterWidth::Wide:
			s = "u32string";
			break;
		default:
			break;
	}
	return "std::"s + s;
}

std::string ArrayType::get_serializer_name() const{
	std::stringstream stream;
	stream << "std::array< " << this->inner->get_serializer_name() << ", " << this->length << ">";
	return stream.str();
}

void ArrayType::generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const{
	static const char * const format = R"file({{
	{type} *{new_pointer} = ({type} *){pointer};
	for (size_t {index} = 0; {index} != {length}; {index}++){{
{deserializer}
	}}
}}
)file";
	auto new_pointer = get_unique_varname();
	std::string s = variable_formatter(format)
		<< "new_pointer" << new_pointer
		<< "index" << get_unique_varname()
		<< "type" << this->fully_qualified_name()
		<< "pointer" << pointer_name
		<< "length" << this->length
		<< "deserializer" << this->inner->generate_deserializer(deserializer_name, new_pointer.c_str());
	stream << s;
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
	static const char * const format = R"file({{
	{type} *{new_pointer} = ({type} *){pointer};
	new ({new_pointer}) {type};
	{deserializer_name}.deserialize(*{new_pointer});
}})file";
	auto type = this->get_source_name();
	auto new_pointer = get_unique_varname();
	std::string s = variable_formatter(format)
		<< "type" << type
		<< "new_pointer" << new_pointer
		<< "pointer" << pointer_name
		<< "deserializer_name" << deserializer_name;
	stream << s;
}

void Type::generate_rollbacker(std::ostream &stream, const char *pointer_name) const{
	stream << "{}";
}

void Type::generate_is_serializable(std::ostream &stream) const{
	stream << "return " << (this->get_is_serializable() ? "true" : "false") << ";\n";
}

ArrayType::ArrayType(const std::shared_ptr<Type> &inner, const EasySignedBigNum &n): NestedType(inner){
	if (n < 1)
		throw std::runtime_error("array length must be >= 1");
	this->length = n.make_positive();
}

std::string ArrayType::get_source_name() const{
	return "std::array<" + this->inner->get_source_name() + ", " + this->length.to_string() + ">";
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
R"file(
for (const auto &{index} : {name}){{
{contents}
}}
)file";
	variable_formatter vf(format);
	auto index = get_unique_varname();
	vf << "index" << index << "name" << this_name;
	generate_for_collection(vf, index, { this->inner }, callback);
}

void AssociativeArrayType::generate_pointer_enumerator(generate_pointer_enumerator_callback_t &callback, const std::string &this_name) const{
	static const char *format =
R"file(
for (const auto &{index} : {name}){{
{contents}
}}
)file";
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

std::string UserType::generate_namespace(bool last_colons) const{
	std::string ret;
	for (auto &ns : this->_namespace){
		ret += ns;
		ret += "::";
	}
	if (last_colons)
		return ret;
	return ret.substr(0, ret.size() - 2);
}

std::string UserClass::generate_header() const{
	std::stringstream stream;
	stream << "class " << this->generate_namespace() << this->name << " : ";
	if (!this->base_classes.empty()){
		bool first = true;
		for (auto &base : this->base_classes){
			if (!first)
				stream << ", ";
			else
				first = false;
			stream << "public ";
			if (base.Virtual)
				stream << "virtual ";
			stream << base.Class->fully_qualified_name();
		}
	}else{
		stream << "public ";
		if (this->inherit_Serializable_virtually)
			stream << "virtual ";
		stream << "Serializable";
	}
	stream << "{\n";
	for (auto &el : this->elements)
		stream << el->output() << (el->needs_semicolon() ? ";\n" : "\n");

	static const char * const format =
R"file(
public:
	{name}(DeserializerStream &);
	virtual ~{name}();
	virtual void get_object_node(std::vector<ObjectNode> &) const override;
	virtual void serialize(SerializerStream &) const override;
	virtual std::uint32_t get_type_id() const override;
	virtual TypeHash get_type_hash() const override;
	virtual std::shared_ptr<SerializableMetadata> get_metadata() const override;
	static std::shared_ptr<SerializableMetadata> static_get_metadata();
	static const char *static_get_class_name(){{
		return "{name}";
	}}
	const char *get_class_name() const override{{
		return static_get_class_name();
	}}
}};
)file";
	stream << (std::string)(variable_formatter(format) << "name" << this->name);

	auto ts = this->base_get_type_string();
	std::cout << ts << std::endl
		<< TypeHash(ts).to_string() << std::endl;

	return stream.str();
}

std::string UserClass::generate_destructor() const{
	if (!this->default_destructor)
		return {};
	static const char * const format = "{namespace}{name}::~{name}(){{}}\n";
	return variable_formatter(format)
		<< "namespace" << this->generate_namespace()
		<< "name" << this->name;
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
		stream << "ss.serialize(";
		auto t = casted->get_type();
		auto ut = t->get_underlying_type();
		if (t.get() != ut.get())
			stream << "(" << ut->get_source_name() << ")";
		stream << "(this->" << casted->get_name() << "));\n";
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
			stream << "proxy_constructor<" << type->get_source_name() << ">(ds)";
		stream << ")\n";
	}
	stream << "{}";
}

void UserClass::generate_deserializer(std::ostream &stream, const char *deserializer_name, const char *pointer_name) const{
	static const char * const format = R"file(
{{
	{type} *temp = ({type} *){pointer_name};
	new (temp) {type}({dn});
}}
)file";

	std::string s = variable_formatter(format)
		<< "type" << this->get_source_name()
		<< "pointer_name" << pointer_name
		<< "dn" << deserializer_name;
	stream << s;
}

void UserClass::generate_rollbacker(std::ostream &stream, const char *pointer_name) const{
	static const char *const format = R"file(
{{
	{type} *temp = ({type} *){pointer_name};
	temp->rollback_deserialization();
}}
)file";
	std::string s = variable_formatter(format)
		<< "type" << this->get_source_name()
		<< "pointer_name" << pointer_name;
	stream << s;
}

bool UserClass::get_is_serializable() const{
	return true;
}

void UserClass::generate_source(std::ostream &stream) const{
	static const char * const format1 =
R"file(
{namespace}{name}::{name}(DeserializerStream &ds)
{ctor}

{dtor}

void {namespace}{name}::get_object_node(std::vector<ObjectNode> &v) const{{
{gon}
}}

void {namespace}{name}::serialize(SerializerStream &ss) const{{
{ser}
}}

std::uint32_t {namespace}{name}::get_type_id() const{{
{gti}
}}

TypeHash {namespace}{name}::get_type_hash() const{{
{gth}
}}

std::shared_ptr<SerializableMetadata> {namespace}{name}::get_metadata() const{{
return this->static_get_metadata();
}}

std::shared_ptr<SerializableMetadata> {namespace}{name}::static_get_metadata(){{
{gmd}
}}
)file";

	stream << (std::string)(variable_formatter(format1)
		<< "namespace" << this->generate_namespace()
		<< "name" << this->name
		<< "ctor" << this->generate_deserializer()
		<< "dtor" << this->generate_destructor()
		<< "gon" << this->generate_get_object_node2()
		<< "ser" << this->generate_serialize()
		<< "gti" << ("return " + utoa(this->get_type_id()) + ";")
		<< "gth" << this->generate_get_type_hash()
		<< "gmd" << this->generate_get_metadata());
}

UserType::UserType(CppFile &file, std::string name, std::vector<std::string> _namespace)
	: CppElement(file)
	, name(std::move(name))
	, _namespace(std::move(_namespace))
{}

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
	stream << "{class " << this->get_serializer_name();
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
		stream << '(' << (int)casted->get_accessibility() << ',' << casted->get_type()->get_underlying_type()->get_serializer_name() << ',' << casted->get_name() << ')';
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
	
	if (counts.size() > std::numeric_limits<std::uint32_t>::max())
		throw std::overflow_error("too many counts");

	std::vector<std::uint32_t> virtual_classes;
	auto n = (std::uint32_t)counts.size();
	for (std::uint32_t i = 0; i < n; i++)
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

std::string UserClass::generate_forward_declaration() const{
	std::stringstream ret;
	if (!this->_namespace.empty())
		ret << "namespace " << this->generate_namespace(false) << "{\n";
	ret << "class " + this->name + ";\n";
	if (!this->_namespace.empty())
		ret << "}\n";
	return ret.str();
}

void UserClass::no_default_destructor(){
	this->default_destructor = false;
}

UserEnum::UserEnum(CppEvaluationState &state, std::string name, std::vector<std::string> _namespace, std::shared_ptr<Type> underlying_type)
	: UserType(*state.result, std::move(name), std::move(_namespace))
	, underlying_type(std::move(underlying_type))
{
	this->id = state.next_enum_id++;
}

std::string UserEnum::base_get_type_string() const{
	return "{enum" + this->name + ":" + this->underlying_type->base_get_type_string() + "}";
}

void UserEnum::set(std::string name, EasySignedBigNum value, std::vector<std::string> additional_strings){
	auto it1 = this->members_by_name.find(name);
	if (it1 != this->members_by_name.end())
		throw std::runtime_error("enum " + this->name + " contains member " + name + " multiple times");
	auto it2 = this->members_by_value.find(value);
	if (it2 != this->members_by_value.end())
		throw std::runtime_error("enum " + this->name + " contains value " + value.to_string() + " multiple times");

	if (!this->underlying_type->check_value(value))
		throw std::runtime_error("value " + value.to_string() + " is out of bounds for type " + this->underlying_type->get_source_name());

	this->members_by_name[name] = value;
	this->additional_strings_by_name[name] = std::move(additional_strings);
	this->members_by_value[std::move(value)] = std::move(name);
}

std::string UserEnum::generate_forward_declaration() const{
	std::stringstream ret;
	if (!this->_namespace.empty())
		ret << "namespace " << this->generate_namespace(false) << "{\n";
	ret << "enum class " + this->name + " : " + this->underlying_type->get_source_name() + ";\n";
	if (!this->_namespace.empty())
		ret << "}\n";
	return ret.str();
}

std::string UserEnum::generate_header() const{
	static const char *const format = R"file(
enum class {name} : {underlying_type}{{
{members}
}};

template <>
struct get_enum_type_id<{name}>{{
	static const std::uint32_t value = {id};
}};

{stringifier_signature};
)file";

	return variable_formatter(format)
		<< "name" << this->get_source_name()
		<< "underlying_type" << this->underlying_type->get_source_name()
		<< "members" << this->generate_members()
		<< "id" << this->id
		<< "stringifier_signature" << this->get_stringifier_signature()
	;
}

std::string UserEnum::generate_stringifier(){
	static const char *const format = R"file(
{signature}{{
	{return_type} ret = {default_value};
	switch (value){{
{switch_cases}
		default:
			break;
	}}
	return ret;
}}
)file";

	return variable_formatter(format)
		<< "signature" << this->get_stringifier_signature()
		<< "return_type" << this->get_stringifier_return_type()
		<< "default_value" << this->get_stringifier_default_value()
		<< "name" << this->get_source_name()
		<< "switch_cases" << this->generate_stringifier_cases();
}

size_t UserEnum::get_max_additional_strings() const{
	if (this->cached_max_additional_strings)
		return *this->cached_max_additional_strings;
	size_t ret = 0;
	for (auto &[_, as] : this->additional_strings_by_name)
		ret = std::max(ret, as.size());
	this->cached_max_additional_strings = ret;
	return ret;
}

std::string UserEnum::get_stringifier_return_type() const{
	auto n = this->get_max_additional_strings();
	if (!n)
		return "const char *";
	n++;
	return "std::array<const char *," + std::to_string(n) + ">";
}

std::string UserEnum::get_stringifier_signature() const{
	return this->get_stringifier_return_type() + " get_enum_string(" + this->get_source_name() + " value)";
}

std::string UserEnum::get_stringifier_default_value() const{
	auto n = this->get_max_additional_strings();
	if (!n)
		return "nullptr";
	n++;
	std::string ret = "{ ";
	while (n--){
		ret += "nullptr, ";
	}
	ret += "}";
	return ret;
}

std::string UserEnum::generate_stringifier_cases() const{
	std::stringstream ret;
	auto n = this->get_max_additional_strings() + 1;
	for (auto &[_, name] : this->members_by_value){
		ret << "case " << this->get_source_name() << "::" << name << ":\n";
		if (n == 1)
			ret << "\tret = \"" << name << "\";\n";
		else{
			ret << "\tret[0] = \"" << name << "\";\n";
			auto &as = this->additional_strings_by_name.find(name)->second;
			for (size_t i = 0; i < n - 1; i++){
				ret << "\tret[" << i + 1 << "] = ";
				if (i < as.size())
					ret << "\"" << as[i] << "\"";
				else
					ret << "nullptr";
				ret << ";\n";
			}
		}
		ret << "break;\n";
	}
	return ret.str();
}

std::vector<EasySignedBigNum> UserEnum::get_valid_values() const{
	std::vector<EasySignedBigNum> ret;
	ret.reserve(this->members_by_value.size());
	for (auto &[k, v] : this->members_by_value)
		ret.push_back(k);
	return ret;
}

std::string UserEnum::generate_members() const{
	static const char *const format = R"file(
{name} = {value},
)file";

	std::string ret;
	for (auto &[value, name] : this->members_by_value){
		ret += variable_formatter(format)
			<< "name" << name
			<< "value" << value;
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

std::string UserInclude::generate_header() const{
	return "#include "s + (this->relative ? '"' : '<') + this->include + (this->relative ? '"' : '>');
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
			throw std::runtime_error("Invalid switch for CppVersion");
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

	auto filename = this->get_name() + ".generated.hpp";
	auto macro = filename_to_macro(filename);

	std::ofstream file(filename.c_str());

	file <<
		R"file(
#pragma once

)file"
	;
	require_cpp(file, this->minimum_cpp_version());

	std::set<std::string> includes;
	for (auto &c : this->classes)
		c.second->add_headers(includes);
	for (auto &h : includes)
		file << "#include " << h << std::endl;
	file << R"file(#include "serialization_utils.hpp"
#include "Serializable.hpp"
#include "SerializerStream.hpp"
#include "DeserializerStream.hpp"
#include "serialization_utils.inl"

)file"
	;
	for (auto &e : this->elements)
		file << e->generate_inclusion();
	file << std::endl;

	for (auto &e : this->elements)
		file << e->generate_forward_declaration();
	file << std::endl;

	for (auto &e : this->elements)
		file << e->generate_header() << std::endl;

	file << std::endl;
}

std::string generate_get_metadata_signature(){
	return "std::shared_ptr<SerializableMetadata> " + get_get_metadata_function_name() + "()";
}

std::string CppFile::generate_source1(){
	static const char * const pattern =
R"file(
#include "Serializable.hpp"
#include "{name}.generated.hpp"
#include <utility>
#include <cstdint>
#include <memory>
#include <algorithm>

{type_comments}

std::pair<std::uint32_t, TypeHash> {array_name}[] = {{
{type_map}
}};

)file";
	return variable_formatter(pattern)
		<< "name" << this->name
		<< "array_name" << this->get_id_hashes_name()
		<< "type_comments" << this->generate_type_comments()
		<< "type_map" << this->generate_type_map()
	;
}

std::string CppFile::generate_static_get_type_id(){
	static const char * const pattern2 =
R"file(
template <>
struct static_get_type_id<{type}>{{
	static const std::uint32_t value = {value};
}};

)file";

	std::string ret;
	for (auto &[id, type] : this->type_map){
		ret += variable_formatter(pattern2)
			<< "type" << type->get_source_name()
			<< "value" << type->get_type_id();
	}

	return ret;
}

void CppFile::generate_source(){
	this->assign_type_ids();

	std::ofstream file((this->get_name() + ".generated.cpp").c_str());

	typedef std::string (CppFile::*generator_f)();
	static const generator_f generators[] = {
		&CppFile::generate_source1,
		&CppFile::generate_static_get_type_id,
		&CppFile::generate_allocator,
		&CppFile::generate_constructor,
		&CppFile::generate_rollbacker,
		&CppFile::generate_is_serializable,
		&CppFile::generate_dynamic_cast,
		&CppFile::generate_generic_pointer_classes_and_implementations,
		&CppFile::generate_pointer_allocator,
		&CppFile::generate_cast_categorizer,
		&CppFile::generate_enum_checker,
		&CppFile::generate_enum_stringifiers,
		&CppFile::generate_get_metadata,
	};

	for (auto f : generators)
		file << (this->*f)() << std::endl;

	for (auto &e : this->elements){
		auto Class = std::dynamic_pointer_cast<UserClass>(e);
		if (!Class)
			continue;
		Class->generate_source(file);
		file << std::endl;
	}
}

std::string CppFile::generate_sizes(){
	std::stringstream ret;
	for (auto &kv : this->type_map){
		if (kv.second->is_abstract()){
			ret << "0, ";
			continue;
		}
		ret << "sizeof(" << kv.second->get_source_name() << "), ";
	}

	return ret.str();
}

std::string CppFile::generate_allocator(){
	static const char * const pattern =
R"file(
void *{name}(std::uint32_t type){{
	static const size_t sizes[] = {{
		{sizes}
	}};
	type--;
	if (!sizes[type])
		return nullptr;
	return ::operator new (sizes[type]);
}}
)file";

	return variable_formatter(pattern) << "name" << get_allocator_function_name() << "sizes" << this->generate_sizes();
}

std::string CppFile::generate_deserializers(){
	std::stringstream stream;
	for (auto &kv : this->type_map){
		if (kv.second->is_abstract()){
			stream << "nullptr,\n";
			continue;
		}
		stream << "[](void *s, DeserializerStream &ds)";
		kv.second->generate_deserializer(stream, "ds", "s");
		stream << ",\n";
	}
	return stream.str();
}

std::string CppFile::generate_constructor(){
	static const char * const format =
R"file(
void {name}(std::uint32_t type, void *s, DeserializerStream &ds){{
	typedef void (*constructor_f)(void *, DeserializerStream &);
	static const constructor_f constructors[] = {{
		{constructors}
	}};
	type--;
	if (!constructors[type])
		return;
	return constructors[type](s, ds);
}}
)file";
	return variable_formatter(format)
		<< "name" << get_constructor_function_name()
		<< "constructors" << this->generate_deserializers()
	;
}

std::string CppFile::generate_rollbackers(){
	std::stringstream stream;
	for (auto &kv : this->type_map) {
		if (kv.second->is_abstract()) {
			stream << "nullptr,\n";
			continue;
		}
		stream << "[](void *s)";
		kv.second->generate_rollbacker(stream, "s");
		stream << ",\n";
	}
	return stream.str();
}

std::string CppFile::generate_rollbacker(){
	static const char * const format =
R"file(
void {name}(std::uint32_t type, void *s){{
	typedef void (*rollbacker_f)(void *);
	static const rollbacker_f rollbackers[] = {{
		{rollbackers}
	}};
	type--;
	if (!rollbackers[type])
		return;
	return rollbackers[type](s);
}}
)file";
	return variable_formatter(format)
		<< "name" << get_rollbacker_function_name()
		<< "rollbackers" << this->generate_rollbackers()
	;
}

std::string CppFile::generate_is_serializable(){
	std::vector<std::uint32_t> flags;
	int bit = 0;
	int i = 0;
	for (auto &kv : this->type_map){
		std::uint32_t u = kv.second->get_is_serializable();
		if (kv.second->get_type_id() != ++i)
			throw std::runtime_error("Unknown program state!");
		u <<= bit;
		if (!bit)
			flags.push_back(u);
		else
			flags.back() |= u;
		bit = (bit + 1) % 32;
	}


	static const char * const format = 
R"file(
bool {name}(std::uint32_t type){{
	static const std::uint32_t is_serializable[] = {{ {array} }};
	type--;
	return (is_serializable[type / 32] >> (type % 32)) & 1;
}}
)file";

	std::stringstream temp;
	for (auto u : flags)
		temp << "0x" << std::hex << std::setw(8) << std::setfill('0') << (int)u << ", ";

	return variable_formatter(format)
		<< "name" << get_is_serializable_function_name()
		<< "array" << temp.str()
	;
}

std::string CppFile::generate_dynamic_casts(){
	std::stringstream stream;
	for (auto &kv : this->type_map){
		if (!kv.second->is_serializable()){
			stream << "nullptr,\n";
			continue;
		}
		stream << "[](void *p){ return dynamic_cast<Serializable *>((" << kv.second->get_source_name() << " *)p); },\n";
	}
	return stream.str();
}

std::string CppFile::generate_dynamic_cast(){
	static const char * const format =
R"file(
Serializable *{name}(void *src, std::uint32_t type){{
	typedef Serializable *(*cast_f)(void *);
	static const cast_f casts[] = {{
{casts}
	}};
	type--;
	if (!casts[type])
		return nullptr;
	return casts[type](src);
}}
)file";
	return variable_formatter(format)
		<< "name" << get_dynamic_cast_function_name()
		<< "casts" << this->generate_dynamic_casts()
	;
}

std::string CppFile::generate_generic_pointer_classes(){
	static const char * const format =
R"file(
class RawPointer{name} : public GenericPointer{{
	std::unique_ptr<{fqn} *> real_pointer;
public:
	RawPointer{name}(void *p){{
		this->real_pointer.reset(new {fqn} *(({fqn} *)p));
		this->pointer = this->real_pointer.get();
	}}
	~RawPointer{name}() = default;
	void release() override{{}}
	std::unique_ptr<GenericPointer> cast(std::uint32_t type) override;
}};

class SharedPtr{name} : public GenericPointer{{
	std::unique_ptr<std::shared_ptr<{fqn}>> real_pointer;
public:
	SharedPtr{name}(void *p){{
		this->real_pointer.reset(new std::shared_ptr<{fqn}>(({fqn} *)p, conditional_deleter<{fqn}>()));
		this->pointer = this->real_pointer.get();
	}}
	template <typename T>
	SharedPtr{name}(const std::shared_ptr<T> &p){{
		this->real_pointer.reset(new std::shared_ptr<{fqn}>(std::static_pointer_cast<{fqn}>(p)));
		this->pointer = this->real_pointer.get();
	}}
	~SharedPtr{name}() = default;
	void release() override{{
		std::get_deleter<conditional_deleter<{fqn}>> (*this->real_pointer)->disable();
	}}
	std::unique_ptr<GenericPointer> cast(std::uint32_t type) override;
}};

class UniquePtr{name} : public GenericPointer{{
    std::unique_ptr<std::unique_ptr<{fqn}>> real_pointer;
public:
    UniquePtr{name}(void *p){{
        this->real_pointer.reset(new std::unique_ptr<{fqn}>(({fqn} *)p));
        this->pointer = this->real_pointer.get();
	}}
    ~UniquePtr{name}() = default;
    void release() override{{
        this->real_pointer->release();
	}}
    std::unique_ptr<GenericPointer> cast(std::uint32_t type) override;
}};
)file";

	std::string ret;
	for (auto &kv1 : this->type_map){
		if (!kv1.second->is_serializable())
			continue;
		auto uc1 = static_cast<UserClass *>(kv1.second);

		ret += variable_formatter(format)
			<< "name" << uc1->get_name()
			<< "fqn" << uc1->fully_qualified_name()
		;
	}
	return ret;
}

std::string CppFile::generate_cast_cases(UserClass &uc1, const char *format){
	std::string ret;
	for (auto &kv2 : this->type_map){
		if (!kv2.second->is_serializable())
			continue;
		auto uc2 = static_cast<UserClass *>(kv2.second);
		if (!uc1.is_subclass_of(*uc2))
			continue;
		ret += variable_formatter(format)
			<< "name" << uc2->get_name()
			<< "id" << uc2->get_type_id();
	}
	return ret;
}

std::string CppFile::generate_raw_pointer_cast_cases(UserClass &uc1){
	static const char * const format =
R"file(
		case {id}:
			return std::unique_ptr<GenericPointer>(new RawPointer{name}(*this->real_pointer));
)file";
	return this->generate_cast_cases(uc1, format);
}

std::string CppFile::generate_shared_ptr_cast_cases(UserClass &uc1){
	static const char * const format =
R"file(
		case {id}:
			return std::unique_ptr<GenericPointer>(new SharedPtr{name}(*this->real_pointer));
)file";
	return this->generate_cast_cases(uc1, format);
}

std::string CppFile::generate_unique_ptr_cast_cases(UserClass &uc1){
	static const char * const format =
R"file(
		case {id}:
			throw std::runtime_error("Multiple unique pointers to single object detected.");
)file";
	return this->generate_cast_cases(uc1, format);
}

std::string CppFile::generate_generic_pointer_class_implementations(){
	static const char * const format =
R"file(
std::unique_ptr<GenericPointer> RawPointer{name}::cast(std::uint32_t type){{
	switch (type){{
{switch_cases_RawPointer}
		default:
			return {{}};
	}}
}}

std::unique_ptr<GenericPointer> SharedPtr{name}::cast(std::uint32_t type){{
	switch (type){{
{switch_cases_SharedPtr}
		default:
			return {{}};
	}}
}}

std::unique_ptr<GenericPointer> UniquePtr{name}::cast(std::uint32_t type){{
	switch (type){{
{switch_cases_UniquePtr}
		default:
			return {{}};
	}}
}}
)file";

	std::string ret;
	for (auto &kv1 : this->type_map){
		if (!kv1.second->is_serializable())
			continue;
		auto uc1 = static_cast<UserClass *>(kv1.second);
		auto name = uc1->get_name();

		ret += variable_formatter(format)
			<< "name" << name
			<< "switch_cases_RawPointer" << this->generate_raw_pointer_cast_cases(*uc1)
			<< "switch_cases_SharedPtr" << this->generate_shared_ptr_cast_cases(*uc1)
			<< "switch_cases_UniquePtr" << this->generate_unique_ptr_cast_cases(*uc1)
		;
	}

	return ret;
}

std::string CppFile::generate_generic_pointer_classes_and_implementations(){
	return
		"namespace {\n" +
		this->generate_generic_pointer_classes() + "\n" +
		this->generate_generic_pointer_class_implementations() + "\n"
		"}\n";
}

std::string CppFile::generate_allocator_cases(const char *format){
	std::string ret;
	for (auto &kv : this->type_map){
		if (!kv.second->is_serializable())
			continue;
		auto uc = static_cast<UserClass *>(kv.second);
		auto name = uc->get_name();
		ret += variable_formatter(format) << "name" << uc->get_name() << "id" << uc->get_type_id();
	}
	return ret;
}

std::string CppFile::generate_raw_pointer_allocator_cases(){
	static const char * const format =
R"file(
				case {id}:
					return std::unique_ptr<GenericPointer>(new RawPointer{name}(p));
)file";
	return this->generate_allocator_cases(format);
}

std::string CppFile::generate_shared_pointer_allocator_cases(){
	static const char * const format =
R"file(
				case {id}:
					return std::unique_ptr<GenericPointer>(new SharedPtr{name}(p));
)file";
	return this->generate_allocator_cases(format);
}

std::string CppFile::generate_unique_ptr_allocator_cases(){
	static const char * const format =
R"file(
				case {id}:
					return std::unique_ptr<GenericPointer>(new UniquePtr{name}(p));
)file";
	return this->generate_allocator_cases(format);
}

std::string CppFile::generate_pointer_allocator(){
	static const char * const format =
R"file(
std::unique_ptr<GenericPointer> {name}(std::uint32_t type, PointerType pointer_type, void *p){{
	switch (pointer_type){{
		case PointerType::RawPointer:
			switch (type){{
{switch_cases_RawPointer}
				default:
					return std::unique_ptr<GenericPointer>();
			}}
			break;
		case PointerType::SharedPtr:
			switch (type){{
{switch_cases_SharedPtr}
				default:
					return std::unique_ptr<GenericPointer>();
			}}
			break;
		case PointerType::UniquePtr:
			switch (type){{
{switch_cases_UniquePtr}
				default:
					return std::unique_ptr<GenericPointer>();
			}}
			break;
		default:
			throw std::runtime_error("Internal error. Unknown pointer type.");
	}}
}}
)file";

	return variable_formatter(format)
		<< "name" << get_allocate_pointer_function_name()
		<< "switch_cases_RawPointer" << this->generate_raw_pointer_allocator_cases()
		<< "switch_cases_SharedPtr" << this->generate_shared_pointer_allocator_cases()
		<< "switch_cases_UniquePtr" << this->generate_unique_ptr_allocator_cases()
	;
}

std::string CppFile::generate_cast_categories(unsigned max_type){
	static const char * const trivial = "\t\tCastCategory::Trivial,\n";
	static const char * const invalid = "\t\tCastCategory::Invalid,\n";
	static const char * const complex = "\t\tCastCategory::Complex,\n";
	std::string ret;
	for (unsigned src = 1; src <= max_type; src++){
		for (unsigned dst = 1; dst <= max_type; dst++){
			if (src == dst){
				ret += trivial;
				continue;
			}
			auto src_t = this->type_map[src];
			auto dst_t = this->type_map[dst];
			if (!src_t->is_serializable() || !dst_t->is_serializable()){
				ret += invalid;
				continue;
			}
			auto src_class = static_cast<UserClass *>(src_t);
			auto dst_class = static_cast<UserClass *>(dst_t);
			if (!src_class->is_subclass_of(*dst_class)){
				ret += invalid;
				continue;
			}
			if (src_class->is_trivial_class()){
				ret += trivial;
				continue;
			}
			ret += complex;
		}
	}
	return ret;
}

std::string CppFile::generate_cast_categorizer(){
	static const char * const format =
R"file(
CastCategory {name}(std::uint32_t src_type, std::uint32_t dst_type){{
	if (src_type < 1 || dst_type < 1 || src_type > {max_type} || dst_type > {max_type})
		return CastCategory::Invalid;
	if (src_type == dst_type)
		return CastCategory::Trivial;
	static const CastCategory categories[] = {{
{categories}
	}};
	src_type--;
	dst_type--;
	return categories[dst_type + src_type * {max_type}];
}}
)file";

	unsigned max_type = 0;
	for (auto &kv : this->type_map)
		max_type = std::max(max_type, kv.first);

	return variable_formatter(format)
		<< "name" << get_categorize_cast_function_name()
		<< "categories" << this->generate_cast_categories(max_type)
		<< "max_type" << max_type
	;
}

std::string to_list(const std::vector<EasySignedBigNum> &values){
	std::stringstream ret;
	for (auto &i : values)
		ret << i << ", ";
	return ret.str();
}

std::string CppFile::generate_enum_checkers(){
	static const char * const format = R"file(
[](const void *void_value){{
	static const {type} valid_values[] = {{ {values} }};
	auto value = *(const {type} *)void_value;
	auto end = valid_values + {length};
	auto it = std::lower_bound(valid_values, end, value);
	return it != end && *it == value;
}},
)file";

	std::string ret;
	std::vector<std::shared_ptr<UserEnum>> sorted;
	sorted.reserve(this->enums.size());
	for (auto &[_, e] : this->enums)
		sorted.push_back(e);
	std::sort(sorted.begin(), sorted.end(), [](const auto &left, const auto &right){
		return left->get_id() < right->get_id();
	});
	size_t i = 0;
	for (auto &e : sorted){
		i++;
		if (e->get_id() != i)
			throw std::runtime_error("unresolvable condition: can't generate enum checkers");

		auto values = e->get_valid_values();

		ret += variable_formatter(format)
			<< "type" << e->get_underlying_type()->get_source_name()
			<< "values" << to_list(values)
			<< "length" << values.size();
	}

	return ret;
}

std::string CppFile::generate_enum_checker(){
	static const char *const empty = R"file(
bool {name}(std::uint32_t enum_type_id, const void *value){{
	return true;
}}
)file";

	static const char * const format = R"file(
bool {name}(std::uint32_t enum_type_id, const void *value){{
	if (enum_type_id < 1 || enum_type_id > {max_type})
		return false;
	typedef bool (*checker)(const void *);
	static const checker checkers[] = {{
{checkers}
	}};
	enum_type_id--;
	return checkers[enum_type_id](value);
}}
)file";

	if (this->enums.empty())
		return variable_formatter(empty)
			<< "name" << get_check_enum_function_name()
		;

	return variable_formatter(format)
		<< "name" << get_check_enum_function_name()
		<< "checkers" << this->generate_enum_checkers()
		<< "max_type" << this->enums.size()
	;
}

std::string CppFile::generate_enum_stringifiers(){
	std::string ret;
	for (auto &[_, e] : this->enums)
		ret += e->generate_stringifier();
	return ret;
}

std::string CppFile::generate_type_comments(){
	std::stringstream ret;
	for (auto &[id, type] : this->type_map)
		ret << "// " << id << ": " << type->get_source_name() << std::endl;
	return ret.str();
}

std::string CppFile::generate_type_map(){
	static const char * const format =
		"// {string}\n"
		"{{ {id}, TypeHash({hash}) }},\n"
	;
	std::string ret;
	for (auto &[id, type] : this->type_map){
		ret += variable_formatter(format)
			<< "string" << type->base_get_type_string()
			<< "id" << id
			<< "hash" << type->get_type_hash().to_string();
	}
	return ret;
}

std::string CppFile::get_id_hashes_name(){
	return this->get_name() + "_id_hashes";
}

std::string CppFile::generate_get_metadata(){
	static const char * const format2 = R"file(
{get_metadata_signature}{{
	std::shared_ptr<SerializableMetadata> ret(new SerializableMetadata);
	ret->set_functions(
		{allocator},
		{constructor},
		{rollbacker},
		{is_serializable},
		{dynamic_cast},
		{allocate_pointer},
		{categorize_cast},
		{check_enum}
	);
	for (auto &[id, hash] : {array_name})
		ret->add_type(id, hash);
	return ret;
}}
)file";

	return variable_formatter(format2)
		<< "get_metadata_signature" << generate_get_metadata_signature()
		<< "allocator" << get_allocator_function_name()
		<< "constructor" << get_constructor_function_name()
		<< "rollbacker" << get_rollbacker_function_name()
		<< "is_serializable" << get_is_serializable_function_name()
		<< "dynamic_cast" << get_dynamic_cast_function_name()
		<< "allocate_pointer" << get_allocate_pointer_function_name()
		<< "categorize_cast" << get_categorize_cast_function_name()
		<< "check_enum" << get_check_enum_function_name()
		<< "array_name" << this->get_id_hashes_name()
	;
}

void CppFile::mark_virtual_inheritances(){
	auto serializable_type_id = this->assign_type_ids();
	for (auto &uc : this->classes)
		uc.second->mark_virtual_inheritances(serializable_type_id);
}

void EvaluationResult::add(std::shared_ptr<CppFile> cpp){
	this->cpps.emplace_back(std::move(cpp));
}

void EvaluationResult::add(std::string name, std::vector<std::shared_ptr<TypeOrNamespaceNonTerminal>> d){
	if (this->decls.find(name) != this->decls.end())
		throw std::runtime_error("decl " + name + " already defined");
	this->decls[std::move(name)] = std::move(d);
}

void VerbatimContainer::add_verbatim(std::string name, std::string content){
	if (this->verbatim_declarations.find(name) != this->verbatim_declarations.end())
		throw std::runtime_error("verbatim block " + name + " already defined");
	this->verbatim_declarations[std::move(name)] = std::move(content);
}

void EvaluationResult::generate(){
	for (auto &cpp : this->cpps){
		random_function_name = 0;
		cpp->generate_header();
		cpp->generate_source();
	}
}
