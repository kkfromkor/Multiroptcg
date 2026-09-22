// Native unit regression for the production packet parser and token helpers.
// Does not open a socket or claim end-to-end room/server validation.
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <cstdlib>
#include <iostream>
#include <set>
#include <openssl/rand.h>

namespace {
int randomResult = 1;
int TestRandomBytes(unsigned char* bytes, int count) {
	return randomResult == 1 ? RAND_bytes(bytes, count) : randomResult;
}
}
// Exercise the actual helper body, injecting only its external RNG dependency
// so both documented failure return values can be tested without altering it.
#define RAND_bytes TestRandomBytes
#include "../src/Multirole/InviteToken.hpp"
#undef RAND_bytes
#include "../src/Multirole/YGOPro/CTOSMsg.hpp"

namespace {
unsigned checks = 0;
void Check(bool value, const char* label) {
	++checks;
	if(!value) { std::cerr << "FAIL " << label << '\n'; std::exit(1); }
}
YGOPro::CTOSMsg Packet(YGOPro::CTOSMsg::MsgType type, const void* body, std::size_t count) {
	YGOPro::CTOSMsg msg;
	const auto length = static_cast<YGOPro::CTOSMsg::LengthType>(count + 1U);
	std::memcpy(msg.Data(), &length, sizeof(length));
	msg.Data()[2U] = static_cast<uint8_t>(type);
	if(count) std::memcpy(msg.Body(), body, count);
	return msg;
}
}

int main() {
	using YGOPro::CTOSMsg;
	using namespace Ignis::Multirole;
	CTOSMsg boundary;
	boundary.Data()[2U] = static_cast<uint8_t>(CTOSMsg::MsgType::READY);
	const auto SetWireLength = [&](uint16_t length) {
		std::memcpy(boundary.Data(), &length, sizeof(length));
	};
	SetWireLength(0U);
	Check(!boundary.IsHeaderValid(), "zero wire length rejected before body read");
	SetWireLength(0xFFFFU);
	Check(!boundary.IsHeaderValid(), "negative signed wire length rejected before body read");
	SetWireLength(1U);
	Check(boundary.IsHeaderValid() && boundary.GetLength() == 0, "opcode-only packet remains valid");
	SetWireLength(CTOSMsg::MSG_MAX_LENGTH + 1U);
	Check(boundary.IsHeaderValid() && boundary.GetLength() == CTOSMsg::MSG_MAX_LENGTH, "maximum body length accepted");
	SetWireLength(CTOSMsg::MSG_MAX_LENGTH + 2U);
	Check(!boundary.IsHeaderValid(), "one byte beyond maximum body rejected");
	CTOSMsg::JoinGameInvite wire{};
	wire.join.id = 0x01020304U;
	wire.join.pass[0] = 'p';
	wire.join.version = {{41U, 0U}, {10U, 0U}};
	const auto legacy = Packet(CTOSMsg::MsgType::JOIN_GAME, &wire.join, sizeof(wire.join));
	Check(legacy.IsHeaderValid(), "legacy opcode remains valid");
	Check(legacy.GetJoinGame().has_value(), "legacy 52-byte body accepted");
	Check(legacy.GetJoinGame()->id == 0x01020304U && legacy.GetJoinGame()->pass[0] == 'p', "legacy fields preserved");
	Check(!legacy.GetJoinGameInvite(), "legacy body cannot become invite");
	Check(legacy.Body()[4U] == 4U && legacy.Body()[7U] == 1U, "legacy id offset and wire byte order");
	Check(!Packet(CTOSMsg::MsgType::JOIN_GAME, &wire, 51U).GetJoinGame(), "short legacy rejected");
	Check(!Packet(CTOSMsg::MsgType::JOIN_GAME, &wire, 53U).GetJoinGame(), "extra legacy byte rejected");

	const auto first = GenerateInviteToken();
	Check(first.has_value(), "real OpenSSL random generation succeeds");
	Check(first->size() == 32U && first->find_first_not_of("0123456789abcdef") == std::string::npos, "canonical lowercase hex32");
	std::memcpy(wire.token, first->data(), sizeof(wire.token));
	const auto invite = Packet(CTOSMsg::MsgType::JOIN_GAME_INVITE, &wire, sizeof(wire));
	Check(invite.IsHeaderValid(), "invite opcode accepted");
	Check(invite.GetJoinGameInvite().has_value(), "exact 84-byte invite accepted");
	Check(invite.GetJoinGameInvite()->join.id == wire.join.id, "invite legacy prefix preserved");
	Check(std::memcmp(invite.GetJoinGameInvite()->token, first->data(), 32U) == 0, "token copied without NUL requirement");
	Check(!invite.GetJoinGame(), "invite payload never accepted as legacy body");
	Check(!Packet(CTOSMsg::MsgType::JOIN_GAME_INVITE, &wire, 52U).GetJoinGameInvite(), "invite cannot omit token");
	Check(!Packet(CTOSMsg::MsgType::JOIN_GAME_INVITE, &wire, 83U).GetJoinGameInvite(), "truncated token rejected");
	std::array<unsigned char, 85U> extra{};
	std::memcpy(extra.data(), &wire, sizeof(wire));
	Check(!Packet(CTOSMsg::MsgType::JOIN_GAME_INVITE, extra.data(), extra.size()).GetJoinGameInvite(), "extra token byte rejected");
	Check(!Packet(static_cast<CTOSMsg::MsgType>(0xF2U), &wire, sizeof(wire)).IsHeaderValid(), "other unknown opcode stays rejected");
	Check(MatchesInviteToken(*first, *first), "matching room lifetime token accepted");
	Check(!MatchesInviteToken({}, *first), "uninitialized room token rejected");
	Check(!MatchesInviteToken(*first, first->substr(1)), "short token rejected");
	Check(!MatchesInviteToken(*first, *first + '0'), "long token rejected");
	auto changed = *first; changed[0] = changed[0] == '0' ? '1' : '0';
	Check(!MatchesInviteToken(*first, changed), "one changed nibble rejected");
	auto embeddedNull = *first; embeddedNull[10] = '\0';
	Check(!MatchesInviteToken(*first, embeddedNull), "embedded NUL does not truncate comparison");
	const auto replacement = GenerateInviteToken();
	Check(replacement.has_value() && !MatchesInviteToken(*replacement, *first), "new lifetime does not accept old token");
	std::set<std::string> generated;
	for(unsigned i = 0U; i < 128U; ++i) {
		const auto token = GenerateInviteToken();
		Check(token && generated.emplace(*token).second, "bounded real RNG sample has no duplicate");
	}
	randomResult = 0;
	Check(!GenerateInviteToken(), "RNG failure returns no token");
	randomResult = -1;
	Check(!GenerateInviteToken(), "unsupported RNG returns no token");
	std::cout << "PASS " << checks << " checks; parser/token unit scope; no server/network execution\n";
}
