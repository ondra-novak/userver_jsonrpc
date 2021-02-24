

#pragma once
#include <imtjson/rpc.h>
#include <imtjson/string.h>
#include <shared/logOutput.h>
#include <userver/http_server.h>
#include <userver_jsonrpc/rpcServer.h>
#include <mutex>
#include <vector>

namespace userver {

using namespace json;

class RpcHttpServer: public HttpServer, public json::RpcServer {
public:

	struct Config {
		bool enableConsole = true;
		bool enableWS = true;
		bool enableDirect = true;
		std::size_t maxReqSize = ~static_cast<std::size_t>(0);
	};


	void addRPCPath(std::string_view path, const Config &cfg);
	void addStats(std::string_view path, std::function<json::Value()> customStats = nullptr);

	bool onRequest(PHttpServerRequest &req, const std::string_view &vpath) ;


	void setMaxReqSize(std::size_t maxReqSize) {
		this->maxReqSize=maxReqSize;
	}
	void enableConsole(bool e) {
		consoleEnabled = e;
	}

	virtual void logDirect(const std::string_view &) {};

protected:

	struct MethodStats {
		json::String name;
		std::size_t requests;
		std::uint64_t mstime;
	};

	std::vector<MethodStats> stats;
	std::mutex statsLock;

	void reportRequest(const json::String &methodName, std::uint64_t  mstime);


	static bool cmpMethod(const MethodStats &a, const MethodStats &b);


	virtual bool onConnect(Stream &s) override;

	void processDirect(std::shared_ptr<Stream> s);
	void processDirectAsync(std::shared_ptr<Stream> s);

	std::size_t maxReqSize=10*1024*1024;
	bool consoleEnabled = false;
	bool enableDirect = true;
};



} /* namespace hflib */
