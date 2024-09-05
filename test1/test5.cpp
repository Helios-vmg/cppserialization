#include "test5_client.generated.hpp"
#include "test5_server.generated.hpp"
#include "handler.hpp"
#include "util.hpp"
#include <random>

extern const std::string valid_filename = "c:/foo.txt";

std::string make_request(bool size, bool fail){
	std::unique_ptr<client::requests::Request> req;
	if (!size){
		auto temp = std::make_unique<client::requests::GetFile>();
		temp->name = !fail ? valid_filename : "c:/bar.txt";
		req = std::move(temp);
	}else{
		auto temp = std::make_unique<client::requests::GetFileSize>();
		temp->name = !fail ? valid_filename : "c:/bar.txt";
		req = std::move(temp);
	}
	return serialize(*req);
}

std::string handle_request(RequestHandler &handler, const std::string &serialized){
	auto request = deserialize<server::requests::Request>(serialized);
	auto response = request->handle(handler);
	return serialize(*response);
}

void test5(std::uint32_t){
	RequestHandler handler;
	auto res = deserialize<client::responses::Response>(handle_request(handler, make_request(false, false)));
	test_assertion(res->to_string() == "[buffer:42]", "Failed check #1");
	res = deserialize<client::responses::Response>(handle_request(handler, make_request(false, true)));
	test_assertion(res->to_string() == "(void)", "Failed check #2");
	test_assertion(res->error == "file not found", "Failed check #3");
	res = deserialize<client::responses::Response>(handle_request(handler, make_request(true, false)));
	test_assertion(res->to_string() == "42", "Failed check #4");
	res = deserialize<client::responses::Response>(handle_request(handler, make_request(true, true)));
	test_assertion(res->to_string() == "(void)", "Failed check #5");
	test_assertion(res->error == "file not found", "Failed check #6");
}
