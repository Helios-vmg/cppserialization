#include "negotiator.hpp"
#include <stdexcept>

namespace type_negotiation{

std::optional<std::map<TypeHash, std::uint32_t>> get_first_message(const Serializable &obj){
	std::map<TypeHash, std::uint32_t> ret;
	for (auto &[id, hash] : obj.get_metadata()->get_known_types()){
		if (ret.find(hash) != ret.end())
			return {};
		ret[hash] = id;
	}
	return ret;
}

std::optional<TypeMapping> process_first_message(const std::map<TypeHash, std::uint32_t> &fm_a, const Serializable &obj){
	auto ofm = get_first_message(obj);
	if (!ofm)
		return {};
	auto fm_b = std::move(*ofm);
	TypeMapping ret;
	std::uint32_t protocol_id = 1;
	for (auto &[hash, id] : fm_a){
		auto it = fm_b.find(hash);
		if (it == fm_b.end())
			continue;
		ret.for_party_a.emplace_back(id, protocol_id);
		ret.for_party_b.emplace_back(it->second, protocol_id);
		protocol_id++;
	}
	return ret;
}

}
