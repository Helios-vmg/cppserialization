#pragma once

#include "test5_server.generated.hpp"

using server::responses::Response;

class RequestHandler{
public:
	std::unique_ptr<Response> handle(const server::requests::GetFile &);
	std::unique_ptr<Response> handle(const server::requests::GetFileSize &);
};
