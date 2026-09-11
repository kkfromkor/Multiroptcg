#include "DeckLimitProvider.hpp"

#include <cstdlib> // std::strtoul, std::strtol
#include <fstream>
#include <mutex>
#include <string>

#include "LogHandler.hpp"
// 별도 로그 싱크 설정(config.json logHandler.*)을 요구하지 않도록 금제
// 프로바이더의 서비스 타입으로 기록한다 — 기존 서버 config.json 무수정 배포.
#define LOG_INFO(...) lh.Log(ServiceType::BANLIST_PROVIDER, Level::INFO, __VA_ARGS__)
#define LOG_ERROR(...) lh.Log(ServiceType::BANLIST_PROVIDER, Level::ERROR, __VA_ARGS__)
#include "../I18N.hpp"

namespace Ignis::Multirole
{

Service::DeckLimitProvider::DeckLimitProvider(Service::LogHandler& lh, std::string_view fnRegexStr) :
	lh(lh),
	fnRegex(fnRegexStr.data())
{}

std::size_t Service::DeckLimitProvider::LimitFor(uint32_t code, std::size_t fallback) const noexcept
{
	std::shared_lock lock(mLimits);
	if(auto search = limits.find(code); search != limits.end())
		return search->second;
	return fallback;
}

void Service::DeckLimitProvider::OnAdd(const std::filesystem::path& path, const PathVector& fileList)
{
	LoadLimits(path, fileList);
}

void Service::DeckLimitProvider::OnDiff(const std::filesystem::path& path, const GitDiff& diff)
{
	// 수정된 파일은 removed/added 양쪽에 실린다 — added가 있으면 재적재로 끝.
	if(AnyMatch(diff.added))
	{
		LoadLimits(path, diff.added);
		return;
	}
	// 표가 저장소에서 삭제된 경우: 예외 없음(전 카드 기본 상한) 상태로 되돌린다.
	if(AnyMatch(diff.removed))
	{
		std::scoped_lock lock(mLimits);
		limits.clear();
		LOG_INFO(I18N::DECK_LIMIT_PROVIDER_CLEARED);
	}
}

// private

bool Service::DeckLimitProvider::AnyMatch(const PathVector& fileList) const noexcept
{
	for(const auto& fn : fileList)
		if(std::regex_match(fn.string(), fnRegex))
			return true;
	return false;
}

void Service::DeckLimitProvider::LoadLimits(const std::filesystem::path& path, const PathVector& fileList) noexcept
{
	std::unordered_map<uint32_t, std::size_t> tmp;
	bool loadedAny = false;
	for(const auto& fn : fileList)
	{
		if(!std::regex_match(fn.string(), fnRegex))
			continue;
		const auto fullPath = (path / fn).lexically_normal();
		LOG_INFO(I18N::DECK_LIMIT_PROVIDER_LOADING_ONE, fullPath.string());
		std::ifstream f(fullPath);
		if(!f.is_open())
		{
			LOG_ERROR(I18N::DECK_LIMIT_PROVIDER_COULD_NOT_LOAD_ONE, fullPath.string());
			continue;
		}
		loadedAny = true;
		std::string line;
		while(std::getline(f, line))
		{
			if(!line.empty() && line.back() == '\r')
				line.pop_back();
			if(line.empty() || line[0] == '#')
				continue;
			const auto tab = line.find('\t');
			if(tab == std::string::npos)
				continue;
			const auto code = std::strtoul(line.c_str(), nullptr, 10);
			const auto limit = std::strtol(line.c_str() + tab + 1, nullptr, 10);
			if(code == 0UL || limit <= 0L)
				continue;
			tmp[static_cast<uint32_t>(code)] = static_cast<std::size_t>(limit);
		}
	}
	if(!loadedAny)
		return;
	std::scoped_lock lock(mLimits);
	limits = std::move(tmp);
	LOG_INFO(I18N::DECK_LIMIT_PROVIDER_TOTAL_LOADED, limits.size());
}

} // namespace Ignis::Multirole
