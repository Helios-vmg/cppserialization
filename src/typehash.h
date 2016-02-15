#ifndef TYPEHASH_H
#define TYPEHASH_H

#ifndef HAVE_PRECOMPILED_HEADERS
#include <cstring>
#include <string>
#endif

struct TypeHash{
	unsigned char digest[32];
	TypeHash(){}
	TypeHash(const std::string &type_string);
	std::string to_string() const;
	int cmp(const TypeHash &b) const{
		return memcmp(this->digest, b.digest, 32);
	}
	bool operator<(const TypeHash &b) const{
		return this->cmp(b) < 0;
	}
	bool operator==(const TypeHash &b) const{
		return this->cmp(b) == 0;
	}
	bool operator!=(const TypeHash &b) const{
		return this->cmp(b) != 0;
	}
};

#endif
