/*
 * rpcServer.cpp
 *
 *  Created on: 31. 10. 2017
 *      Author: ondra
 */

#include "rpcServer.h"
#include <imtjson/serializer.h>
#include <imtjson/parser.h>
#include <userver/query_parser.h>

#include "resources.h"


namespace userver {

template<typename LogFn>
void handleLogging(const Value &v, const RpcRequest &req, LogFn &&logFn) noexcept {
	try {
		Value diagData = req.getDiagData();
		//do not log notifications
		if ((v.type() != json::object || v["method"].defined()) && !diagData.defined()) return;
		Value args = req.getArgs();
		Value method = req.getMethodName();
		Value context = req.getContext();
		if (!diagData.defined() && req.isErrorSent()) {
			diagData = v["error"];
		}
		if (!diagData.defined()) diagData = nullptr;
		if (!context.defined()) context = nullptr;
		Value output = {method,args,context,diagData};
		logFn(output.toString().str());
	} catch (...) {

	}
}


bool RpcHttpServer::onRequest(PHttpServerRequest &req, const std::string_view &vpath)  {
	auto method = req->getMethod();
	QueryParser qp(vpath);
	if (method == "POST") {
		auto p = req.get();
		p->readBodyAsync(maxReqSize, [this, req = std::move(req)](const std::string_view &data) mutable {
			json::Value jrq = json::Value::fromString(data);
			req->setContentType("application/json");
			auto stream = req->send();

			RpcRequest rpcreq = RpcRequest::create(RpcRequest::ParseRequest(jrq),
					[req = std::move(req), stream = std::move(stream), this](json::Value response, json::RpcRequest rpcreq) mutable {
						if (response.defined()) {
							response.serialize([&](char c){stream.putChar(c);});
							handleLogging(response, rpcreq, [&](const std::string_view &str){
								req->log("[RPC] ", str);
							});
							auto revTime = req->getRecvTime();
							this->reportRequest(rpcreq.getMethodName().getString(),
									std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now()-revTime).count());
							if (response["error"].defined() || response["result"].defined()) {
								stream.flushAsync([req = std::move(req)](Stream &s, bool){
									s.closeOutput();
								});
							} else {
								stream.flush();
							}
						} else {
							stream.writeNB("\r\n");
							stream.flush();
						}
						return true;
			}, json::RpcFlags::preResponseNotify);
			exec(rpcreq);
		});
		return true;


	} else if (method == "GET") {
		auto path = qp.getPath();
		if (path.empty() && req->directoryRedir()) return true;
		if (path == "/methods") {
			auto cb = qp["callback"];
			Value methods;
			{
				Array methodsArr;
				enumMethods([&methodsArr](Value v){methodsArr.push_back(v);});
				methods = methodsArr;
			}
			auto m = methods.stringify();

			if (cb.empty()) {
				req->setContentType("application/json");
				req->send(m.str());
			} else {
				req->setContentType("text/javascript");
				String n ({
					cb, "(", m, ");\r\n"
				});
				req->send(n.str());
			}
			return true;
		} else {
			const Resource *selRes = nullptr;
			if (consoleEnabled) {
				if (path == "/index.html" || path == "/") selRes = &client_index_html;
				else if (path == "/styles.css") selRes = &client_styles_css;
			}
			if (path == "/rpc.js") selRes = &client_rpc_js;
			if (selRes) {
				req->setContentType(selRes->contentType);
				req->send(selRes->data);
				return true;
			} else {
				return false;
			}
		}
	}
	return false;
}


bool RpcHttpServer::cmpMethod(const MethodStats &a, const MethodStats &b) {
	return a.name < b.name;
}

void RpcHttpServer::reportRequest(const json::String &methodName, std::size_t mstime) {
	std::lock_guard _(statsLock);
	MethodStats s{methodName,1, mstime};
	auto iter = std::lower_bound(stats.begin(), stats.end(), s, cmpMethod);
	if (iter == stats.end()) {
		stats.push_back(s);
	} else if (iter->name != methodName) {
		stats.insert(iter, s);
	} else {
		iter->requests+=s.requests;
		iter->mstime+=s.mstime;
	}
}


void RpcHttpServer::addStats(std::string_view path, std::function<json::Value()> customStats) {
	addPath(path, [this, customStats = std::move(customStats)](userver::PHttpServerRequest &req, const std::string_view & ){

		json::Value c;
		if (customStats) c = customStats();
		json::Object statObj(c);
		json::Object mstats;

		std::lock_guard _(statsLock);
		for (const auto &x: stats) {
			mstats.set(x.name, json::Object
					("req",x.requests)
					("time", x.mstime)
					("avg",	static_cast<double>(x.mstime)/static_cast<double>(x.requests)));
		}
		statObj.set("server", mstats);

		json::Value resp = statObj;
		auto respStr = resp.stringify();
		req->setContentType("application/json");
		req->send(respStr.str());
		return true;
	});
}

bool RpcHttpServer::onConnect(Stream &s) {
	if (enableDirect) {
		s.readAsync([=](Stream &s, std::string_view data) {
			if (data.empty()) return;
			while (!data.empty() && isspace(data[0])) {
				data = data.substr(1);
			}
			if (data.empty()) {
				onConnect(s);
			}
			else {
				s.putBack(data);
				if (data[0] == '{') {
					processDirect(std::make_shared<Stream>(std::move(s)));
				} else {
					process(std::move(s));
				}
			}
		});
		return true;
	} else {
		return false;
	}
}

void RpcHttpServer::addRPCPath(std::string_view path, const Config &cfg) {
	consoleEnabled = cfg.enableConsole;
	enableDirect = cfg.enableDirect;
	maxReqSize = cfg.maxReqSize;

	addPath(path, [this](PHttpServerRequest &req, const std::string_view &vpath){
		return onRequest(req, vpath);
	});
}

void RpcHttpServer::processDirectAsync(std::shared_ptr<Stream> s) {
	s->readAsync([s,this](std::string_view data){
		if (data.empty()) return;
		while (!data.empty() && isspace(data[0])) {
			data = data.substr(1);
		}
		if (data.empty()) {
			this->processDirectAsync(s);
		}
		else {
			s->putBack(data);
			processDirect(s);
		}
	});
}

void RpcHttpServer::processDirect(std::shared_ptr<Stream> s) {

	auto startTime = std::chrono::system_clock::now();
	json::Value req = json::Value::parse([&]{
		return s->getChar();
	});

	RpcRequest rpcreq = RpcRequest::create(RpcRequest::ParseRequest(req),
			[s,this,startTime](json::Value response, json::RpcRequest rpcreq) mutable {
				response.serialize([&](char c){s->putChar(c);});
				s->flush();
				handleLogging(response, rpcreq, [&](const std::string_view &str){
					this->logDirect(str);
				});
				if (!response["method"].defined()) {
					auto endTime = std::chrono::system_clock::now();
					this->reportRequest(rpcreq.getMethodName().getString(),
							std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count()
					);
				}
				return true;
	}, json::RpcFlags::notify);

	exec(rpcreq);

	processDirectAsync(s);


}

} /* namespace hflib */

