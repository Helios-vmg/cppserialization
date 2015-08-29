#include "stdafx.h"
#include "typehash.h"
#include "sha256.h"
#ifndef HAVE_PRECOMPILED_HEADERS
#include <sstream>
#include <iomanip>
#endif

TypeHash::TypeHash(const std::string &type_string){
	SHA256_CTX ctx;
	sha256_init(&ctx);
	sha256_update(&ctx, (const BYTE *)type_string.c_str(), type_string.size());
	sha256_final(&ctx, this->digest);
}

std::string TypeHash::to_string() const{
	std::stringstream stream;
	stream << "{ ";
	for (auto c : this->digest)
		stream << "0x" << std::hex << std::setw(2) << std::setfill('0') << (unsigned)c << ", ";
	stream << "}";
	return stream.str();
}
