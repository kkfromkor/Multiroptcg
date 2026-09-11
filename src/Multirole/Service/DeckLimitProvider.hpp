#ifndef SERVICE_DECKLIMITPROVIDER_HPP
#define SERVICE_DECKLIMITPROVIDER_HPP
#include "../Service.hpp"

#include <cstddef>
#include <cstdint>
#include <regex>
#include <shared_mutex>
#include <unordered_map>

#include "../IGitRepoObserver.hpp"

namespace Ignis::Multirole
{

// [OPCG] 카드별 덱 투입 상한 예외표 — "룰상, 이 카드는 덱에 몇 장이든 넣을 수
// 있다"(OP01-075 파시피스타·OP08-072 비스킷 병사·OP16-042 임펠다운 수인).
// 클라이언트 deck_manager가 읽는 저장소 파일 deck_limits.txt(<베이스 코드>TAB
// <상한>, # 주석)를 서버 CheckDeck도 그대로 읽어 4장 하드코딩을 데이터로
// 대체한다. 조회 키는 alias 정규화된 베이스 코드(별쇄는 베이스로 합산).
class Service::DeckLimitProvider final : public IGitRepoObserver
{
public:
	DeckLimitProvider(Service::LogHandler& lh, std::string_view fnRegexStr);

	// 표에 있으면 그 상한, 없으면 fallback(호출자의 기본 상한).
	std::size_t LimitFor(uint32_t code, std::size_t fallback) const noexcept;

	// IGitRepoObserver overrides
	void OnAdd(const std::filesystem::path& path, const PathVector& fileList) override;
	void OnDiff(const std::filesystem::path& path, const GitDiff& diff) override;
private:
	Service::LogHandler& lh;
	const std::regex fnRegex;
	std::unordered_map<uint32_t, std::size_t> limits;
	mutable std::shared_mutex mLimits;

	bool AnyMatch(const PathVector& fileList) const noexcept;
	void LoadLimits(const std::filesystem::path& path, const PathVector& fileList) noexcept;
};

} // namespace Ignis::Multirole

#endif // SERVICE_DECKLIMITPROVIDER_HPP
