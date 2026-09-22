#ifndef MULTIROLE_INVITE_TOKEN_HPP
#define MULTIROLE_INVITE_TOKEN_HPP
#include <array>
#include <optional>
#include <string>
#include <string_view>
#include <openssl/rand.h>

namespace Ignis::Multirole
{

// Per-room identity, independent of the duel's deterministic RNG seed.
// RAND_bytes returns 1 only on success; never publish an uninitialized token.
// https://docs.openssl.org/3.0/man3/RAND_bytes/
inline std::optional<std::string> GenerateInviteToken()
{
	std::array<unsigned char, 16U> bytes{};
	if(RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1)
		return std::nullopt;
	constexpr char hex[] = "0123456789abcdef";
	std::string token(32U, '0');
	for(std::size_t i = 0U; i < bytes.size(); ++i)
	{
		token[i * 2U] = hex[bytes[i] >> 4U];
		token[i * 2U + 1U] = hex[bytes[i] & 0x0FU];
	}
	return token;
}

inline bool MatchesInviteToken(std::string_view expected, std::string_view supplied) noexcept
{
	return expected.size() == 32U && supplied.size() == 32U && expected == supplied;
}

} // namespace Ignis::Multirole
#endif // MULTIROLE_INVITE_TOKEN_HPP
