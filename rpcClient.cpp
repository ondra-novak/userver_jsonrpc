#include <imtjson/parser.h>
#include <imtjson/serializer.h>
#include "rpcClient.h"


namespace userver {

void HttpJsonRpcClient::enableAsync(const std::shared_ptr<HttpJsonRpcClient> &inst) {
	inst->async_me = inst;
}


using namespace json;


HttpJsonRpcClient::HttpJsonRpcClient(HttpClientCfg &&cfg, std::string url, json::RpcVersion version)
	:AbstractRpcClient(version)
	,httpc(std::move(cfg))
	,url(url) {
}

void HttpJsonRpcClient::setHeaders(std::vector<HttpClient::HeaderPair> &&headers) {
	headers.push_back({"Accept","application/json"});
	headers.push_back({"Content-Type","application/json"});
	this->headers = std::move(headers);
}

void HttpJsonRpcClient::sendRequest(const Value &id, const json::Value &request) {

	buffer.clear();
	request.serialize([&](char c){buffer.push_back((unsigned char)c);});

	if (headers.empty()) {
		setHeaders({});
	}

	auto me = async_me.lock();
	if (me == nullptr) {
		auto resp = httpc.POST(url, headers, buffer);
		if (resp == nullptr) {
			cancelPendingCall(id, RpcResult::makeError(-1, "", Value()));
		} else {
			parseResponse(id, *resp);
		}
	} else {
		httpc.POST(url, headers, buffer, [id = Value(id), me](std::unique_ptr<HttpClientRequest> &&req){
			me->parseResponseAsync(id, std::move(req));
		});
	}
}

void HttpJsonRpcClient::finishResponse(const json::Value &r) noexcept {
	auto type = processResponse(r);
	switch (type) {
		case AbstractRpcClient::success:
			break;
		case AbstractRpcClient::request:
		case AbstractRpcClient::notification:
			onNotify(RpcNotify(r));
			break;
		case AbstractRpcClient::unexpected:
			onUnexpected(r);
			break;
	}
}

void HttpJsonRpcClient::parseResponse(json::Value id, HttpClientRequest &resp) {
	try {
		if (resp.getStatus() == 200) {
			Stream &b = resp.getResponse();
			Value r = Value::parse([&](){return b.getChar();});
			finishResponse(r);
		} else {
			cancelPendingCall(id, RpcResult::makeError(-1,"HTTP failed",{resp.getStatus(), resp.getStatusMessage()}));
		}
	} catch (std::exception &e) {
		cancelPendingCall(id, RpcResult::makeError(-1,"Response process exception",e.what()));
	}
}

void HttpJsonRpcClient::parseResponseAsync(json::Value id, std::unique_ptr<HttpClientRequest> &&resp) {
	if (resp->getStatus() == 200) {
		Stream s = resp->getResponseBody(std::move(resp));
		s.readToStringAsync(std::string(), 0, [this,id](Stream &s, const std::string &buffer){
			try {
				Value r = Value::fromString(buffer);
				finishResponse(r);
			} catch (std::exception &e) {
				cancelPendingCall(id, RpcResult::makeError(-1,"Response process exception",e.what()));
			}
		});
	} else {
		cancelPendingCall(id, RpcResult::makeError(-1,"HTTP failed",{resp->getStatus(), resp->getStatusMessage()}));
	}

}

}

