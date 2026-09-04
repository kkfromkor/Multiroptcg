#ifndef YGOPRO_BANLIST_PARSER_HPP
#define YGOPRO_BANLIST_PARSER_HPP
#include <unordered_map>

#include "Banlist.hpp"

namespace YGOPro
{

using BanlistHash = uint32_t;
using BanlistMap = std::unordered_map<BanlistHash, BanlistPtr>;

} // namespace YGOPro

#endif // YGOPRO_BANLIST_PARSER_HPP

#ifdef YGOPRO_BANLIST_PARSER_IMPLEMENTATION
#ifndef YGOPRO_BANLIST_PARSER_IMPL_HPP
#define YGOPRO_BANLIST_PARSER_IMPL_HPP
#include <charconv>
#include <fmt/format.h>

namespace YGOPro
{

namespace Detail
{

constexpr const BanlistHash BANLIST_HASH_MAGIC = 0x7DFCEE6A;

constexpr BanlistHash Salt(BanlistHash hash, uint32_t code, int32_t count) noexcept
{
	constexpr uint32_t HASH_MAGIC_1 = 18U;
	constexpr uint32_t HASH_MAGIC_2 = 14U;
	constexpr uint32_t HASH_MAGIC_3 = 27U;
	constexpr uint32_t HASH_MAGIC_4 = 5U;
	return hash ^ ((code << HASH_MAGIC_1) | (code >> HASH_MAGIC_2)) ^
	       ((code << (HASH_MAGIC_3 + count)) | (code >> (HASH_MAGIC_4 - count)));
}

} // namespace Detail

template<typename Stream>
void ParseForBanlists(Stream& stream, BanlistMap& banlists)
{
	BanlistHash hash = Detail::BANLIST_HASH_MAGIC;
	bool whitelist = false;
	Banlist::DictType dict;
	Banlist::PairList pairs; // [OPCG r47]
	auto ConditionallyAdd = [&]()
	{
		if(hash == Detail::BANLIST_HASH_MAGIC)
			return;
		auto banlist = std::make_shared<Banlist>(whitelist, std::move(dict), std::move(pairs));
		banlists.emplace(std::piecewise_construct,
			std::forward_as_tuple(hash),
			std::forward_as_tuple(std::move(banlist))
		);
	};
	std::string l;
	std::size_t lc = 0U;
	auto MakeException = [&lc](std::string_view str)
	{
		return std::runtime_error(fmt::format("line {:d}: {:s}", lc, str));
	};
	while(++lc, std::getline(stream, l))
	{
		if(l.empty())
			continue;
		if(l.find("$whitelist") != std::string::npos)
		{
			whitelist = true;
			continue;
		}
		// [OPCG r47] 금지 페어 지시자 "$pair <A> <B> [<B2> ...] [--주석]": A는 각 B와 같은 덱 불가.
		// 해시에는 반영하지 않는다(클라 LoadLFListSingle과 동일 규약 - 구 버전과 해시 호환).
		// 형식이 어긋난 줄은 리스트 전체를 죽이지 않고 건너뛴다.
		if(l.rfind("$pair", 0U) == 0U)
		{
			std::vector<uint32_t> codes;
			for(std::size_t i = 5U; i < l.size();)
			{
				if(l.compare(i, 2U, "--") == 0)
					break;
				if(l[i] < '0' || l[i] > '9')
				{
					++i;
					continue;
				}
				auto end = l.find_first_not_of("0123456789", i);
				if(end == std::string::npos)
					end = l.size();
				uint32_t code = 0U;
				if(std::from_chars(l.data() + i, l.data() + end, code).ec == std::errc())
					codes.push_back(code);
				i = end;
			}
			for(std::size_t k = 1U; k < codes.size(); ++k)
				if(codes[0U] != 0U && codes[k] != 0U && codes[0U] != codes[k])
					pairs.emplace_back(codes[0U], codes[k]);
			continue;
		}
		switch(l[0U])
		{
		case '!':
		{
			ConditionallyAdd();
			// Reset state
			hash = Detail::BANLIST_HASH_MAGIC;
			whitelist = false;
			dict.clear();
			pairs.clear();
			continue;
		}
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
		{
			uint32_t code;
			int32_t count;

			const auto separator = l.find(' ');
			if(separator == std::string::npos)
				throw MakeException("Card code separator not found");

			{
				const auto [_, error]
				{
					std::from_chars(l.data(), l.data() + separator, code)
				};
				if(error != std::errc())
					throw MakeException("Could not parse code");
				if(code == 0U)
					throw MakeException("Card code cannot be 0");
			}
			{
				static constexpr auto INT_CHARS = "-0123456789";
				const auto begin = l.find_first_of(INT_CHARS, separator);
				if(begin == std::string::npos)
					throw MakeException("Could not find count begin");
				auto end = l.find_first_not_of(INT_CHARS, begin);
				if(end == std::string::npos)
					end = l.size();
				const auto [_, error]
				{
					std::from_chars(l.data() + begin, l.data() + end, count)
				};
				if(error != std::errc())
					throw MakeException("Could not parse count");
			}

			hash = Detail::Salt(hash, code, count);
			dict[code] = count;
			continue;
		}
		}
	}
	ConditionallyAdd();
}

} // namespace YGOPro

#endif // YGOPRO_BANLIST_PARSER_IMPL_HPP
#endif // YGOPRO_BANLIST_PARSER_IMPLEMENTATION
