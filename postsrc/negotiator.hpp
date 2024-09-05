#pragma once

#include <Serializable.hpp>
#include <map>
#include <cstdint>
#include <optional>
#include <vector>
#include <utility>

namespace type_negotiation{

//Called by party A
//If the optional is null, the type system cannot be negotiated, as it contains
//ambiguities.
std::optional<std::map<TypeHash, std::uint32_t>> get_first_message(const Serializable &);

class TypeMapping{
public:
	std::vector<std::pair<std::uint32_t, std::uint32_t>> for_party_a,
		for_party_b;
	TypeMapping() = default;
	TypeMapping(TypeMapping &&) = default;
	TypeMapping &operator=(TypeMapping &&) = default;
};

//Called by party B
//If the optional is null, the type system cannot be negotiated, as it contains
//ambiguities.
std::optional<TypeMapping> process_first_message(const std::map<TypeHash, std::uint32_t> &, const Serializable &);

}
