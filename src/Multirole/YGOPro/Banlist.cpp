#include "Banlist.hpp"

namespace YGOPro
{

Banlist::Banlist(bool whitelist, DictType dict, PairList pairs) noexcept :
	whitelist(whitelist),
	dict(std::move(dict)),
	pairs(std::move(pairs))
{}

bool Banlist::IsWhitelist() const noexcept
{
	return whitelist;
}

const Banlist::DictType& Banlist::Dict() const noexcept
{
	return dict;
}

const Banlist::PairList& Banlist::Pairs() const noexcept
{
	return pairs;
}

} // namespace YGOPro
