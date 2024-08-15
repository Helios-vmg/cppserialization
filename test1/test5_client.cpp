#include "test5_client.generated.h"
#include "test5_client.generated.cpp"
#include <sstream>

namespace client::responses{

std::string Void::to_string() const{
	return "(void)";
}

std::string FileContents::to_string() const{
	std::stringstream ret;
	ret << "[buffer:" << this->contents.size() << "]";
	return ret.str();
}

std::string FileSize::to_string() const{
	return std::to_string(this->file_size);
}

}
