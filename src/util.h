#include <sstream>
#include <string>

inline unsigned get_unique_id(){
	static unsigned ret = 0;
	return ret++;
}

inline std::string utoa(unsigned n){
	std::stringstream stream;
	stream << n;
	return stream.str();
}

inline std::string get_unique_varname(){
	auto id = get_unique_id();
	std::stringstream stream;
	stream << 'i' << id;
	return stream.str();
}
