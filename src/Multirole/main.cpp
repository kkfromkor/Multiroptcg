/**
 *  Project Ignis: Multirole
 *  Licensed under AGPL
 *  Refer to the COPYING file included.
 */
#include <cstdlib> // Exit flags
#include <fstream> // std::ifstream
#include <optional> // std::optional

#include <boost/json/src.hpp>
#include <fmt/format.h>
#include <git2.h>
#include <sqlite3.h>

#include "Instance.hpp"
#include "I18N.hpp"

namespace
{

inline int CreateAndRunServerInstance() noexcept
{
	using namespace Ignis::Multirole;
	std::optional<Instance> server;
	try
	{
		std::ifstream f("config.json");
		boost::json::monotonic_resource mr;
		boost::json::stream_parser p(&mr);
		for(std::string l; std::getline(f, l);)
			p.write(l);
		p.finish();
		server.emplace(p.release());
	}
	catch(const std::exception& e)
	{
		fmt::print(I18N::MAIN_SERVER_INIT_FAILURE, e.what());
		return EXIT_FAILURE;
	}
	return server->Run();
}

} // namespace

int main()
{
	git_libgit2_init();
	// [OPCG 2026-08-26] openssl 전송 libgit2(MSVC/vcpkg 빌드)는 CA 번들이 없으면
	// https 저장소 동기화가 "SSL certificate is invalid"로 죽는다. 실행 폴더의
	// cacert.pem을 자동 인식(없으면 종전 동작 - winhttp/시스템 인증서 빌드 무해).
	if(std::ifstream("./cacert.pem").good())
		git_libgit2_opts(GIT_OPT_SET_SSL_CERT_LOCATIONS, "./cacert.pem", nullptr);
	sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
	sqlite3_initialize();
	int exitFlag = CreateAndRunServerInstance();
	sqlite3_shutdown();
	git_libgit2_shutdown();
	return exitFlag;
}
