#ifndef TYPEHASH_H
#define TYPEHASH_H

#include <cstring>
#include <string>

struct TypeHash{
	unsigned char digest[32];
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
		return !memcmp(this->digest, b.digest, 32);
	}
	bool operator!=(const TypeHash &b) const{
		return this->cmp(b) != 0;
	}
};

#endif
