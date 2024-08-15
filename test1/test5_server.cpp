#include "test5_server.generated.h"
#include "test5_server.generated.cpp"
#include "handler.h"

extern const std::string valid_filename;

namespace server::requests{

#define DEFINE_DOUBLE_DISPATCH(x) \
std::unique_ptr<Response> x::handle(RequestHandler &handler) const{ \
	return handler.handle(*this); \
}

DEFINE_DOUBLE_DISPATCH(GetFile)
DEFINE_DOUBLE_DISPATCH(GetFileSize)

}

std::unique_ptr<Response> RequestHandler::handle(const server::requests::GetFile &req){
	if (req.name != valid_filename){
		auto ret = std::make_unique<server::responses::Void>();
		ret->error = "file not found";
		return ret;
	}
	auto ret = std::make_unique<server::responses::FileContents>();
	ret->contents.resize(42);
	return ret;
}

std::unique_ptr<Response> RequestHandler::handle(const server::requests::GetFileSize &req){
	if (req.name != valid_filename){
		auto ret = std::make_unique<server::responses::Void>();
		ret->error = "file not found";
		return ret;
	}
	auto ret = std::make_unique<server::responses::FileSize>();
	ret->file_size = 42;
	return ret;
}
