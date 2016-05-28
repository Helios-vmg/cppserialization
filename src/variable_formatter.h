#ifndef HAVE_PRECOMPILED_HEADERS
#include <string>
#include <map>
#include <sstream>
#endif

template <typename T>
class basic_variable_formatter{
	typedef std::basic_string<T> str_t;
	str_t main_string;
	bool push_state;
	str_t last_var;
	std::map<str_t, str_t> variables;
public:
	basic_variable_formatter(const str_t &s): main_string(s), push_state(false){}

	template <typename T2>
	basic_variable_formatter<T> &operator<<(const T2 &v){
		if (!this->push_state){
			std::stringstream stream;
			stream << v;
			this->last_var = stream.str();
		}else
			this->add_variable(this->last_var, v);
		this->push_state = !this->push_state;
		return *this;
	}

	operator str_t() const{
		str_t ret;
		auto result = this->eval(ret);
		if (result != Success)
			abort();
		return ret;
	}

	void clear(){
		this->variables.clear();
	}

	void set_string(const str_t &s){
		this->main_string = s;
	}
	void set_variables(const std::map<str_t, str_t> &vars){
		this->variables = vars;
	}
	template <typename T2>
	void add_variable(const str_t &key, const T2 &value){
		std::stringstream stream;
		stream << value;
		this->variables[key] = stream.str();
	}
	void add_variable(const str_t &key, const str_t &value){
		this->variables[key] = value;
	}
	enum EvalResult{
		Success = 0,
		Fail,
		LBraceUnexpected,
		BraceInVariableName,
		VariableNotFound,
		ExtraneousRBrace,
		UnfinishedVariableName,
		InternalError,
	};
	EvalResult eval(str_t &dst) const{
		std::stringstream stream(this->main_string);
		dst.clear();
		enum State{
			Reading,
			LBraceFound,
			RBraceFound,
			ReadingVariableName,
		};
		State state = Reading;
		str_t variable_accum;
		for (size_t i = 0; i < this->main_string.size(); i++){
			T c = this->main_string[i];
			int table_index = 0;
			switch (c){
				case '{':
					table_index += 10;
					break;
				case '}':
					table_index += 20;
					break;
				default:
					table_index += 30;
					break;
			}
			switch (state){
				case Reading:
					table_index += 1;
					break;
				case LBraceFound:
					table_index += 2;
					break;
				case RBraceFound:
					table_index += 3;
					break;
				case ReadingVariableName:
					table_index += 4;
					break;
			}
			switch (table_index){
				case 11:
					state = LBraceFound;
					variable_accum.clear();
					break;
				case 12:
					dst.push_back('{');
					state = Reading;
					break;
				case 13:
					return LBraceUnexpected;
				case 14:
					return BraceInVariableName;
				case 21:
					state = RBraceFound;
					break;
				case 22:
					{
						typename std::map<str_t, str_t>::const_iterator it = this->variables.find(variable_accum);
						if (it == this->variables.end())
							return VariableNotFound;
						dst.append(it->second);
					}
					state = Reading;
					break;
				case 23:
					dst.push_back('}');
					state = Reading;
					break;
				case 24:
					{
						typename std::map<str_t, str_t>::const_iterator it = this->variables.find(variable_accum);
						if (it == this->variables.end())
							return VariableNotFound;
						dst.append(it->second);
					}
					state = Reading;
					break;
				case 31:
					dst.push_back(c);
					break;
				case 32:
					variable_accum.push_back(c);
					state = ReadingVariableName;
					break;
				case 33:
					return ExtraneousRBrace;
				case 34:
					variable_accum.push_back(c);
					break;
			}
		}
		switch (state){
			case Reading:
				return Success;
			case LBraceFound:
				return UnfinishedVariableName;
			case RBraceFound:
				return ExtraneousRBrace;
			case ReadingVariableName:
				return UnfinishedVariableName;
		}
		return InternalError;
	}
};

typedef basic_variable_formatter<char> variable_formatter;
