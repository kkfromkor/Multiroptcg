#ifndef YGOPRO_BANLIST_HPP
#define YGOPRO_BANLIST_HPP
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <utility>
#include <vector>

namespace YGOPro
{

class Banlist final
{
public:
	using DictType = std::unordered_map<uint32_t /*code*/, int32_t /*count*/>;
	// [OPCG r47] 금지 페어 (A, B): 같은 덱에 동시 사용 불가. 금제 파일의 "$pair A B..." 줄.
	using PairList = std::vector<std::pair<uint32_t, uint32_t>>;

	Banlist(bool whitelist, DictType dict, PairList pairs = {}) noexcept;

	bool IsWhitelist() const noexcept;
	const DictType& Dict() const noexcept;
	const PairList& Pairs() const noexcept;
private:
	const bool whitelist;
	DictType dict;
	PairList pairs;
};

using BanlistPtr = std::shared_ptr<Banlist>;

} // namespace Multirole

#endif // YGOPRO_BANLIST_HPP
