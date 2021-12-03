#pragma once

#include <memory>
#include <imtjson/rpc.h>
#include <userver/http_client.h>

namespace userver {



///JSONRPC client using HTTP protocol
/**
 * It can be used to send single JSONRPC command to the server, while
 * server is able to send a response.
 *
 *
 */
class HttpJsonRpcClient: public json::AbstractRpcClient {
public:


	///Construct RPC client
	/**
	 * @param client shared pointer to HttpClient
	 * @param url url to RPC server
	 * @param async set true to perform asynchronous operations.
	 *    set false to perform synchronous operations.
	 * @param vesion JSONRPC version
	 *
	 * Asynchronous operations are not possible when source client
	 * is no AsyncProvider defined.
	 */
	HttpJsonRpcClient(HttpClientCfg &&httpc, std::string url, json::RpcVersion version = json::RpcVersion::ver2);



	virtual void onNotify(const json::RpcNotify &) {}
	virtual void onUnexpected(const json::Value &) {}

	///Enable asynchronous requests - it requires shared ptr of the instance
	static void enableAsync(const std::shared_ptr<HttpJsonRpcClient> &inst);

	void setHeaders(std::vector<HttpClient::HeaderPair> &&headers);
protected:

	HttpClient httpc;
	std::string url;
	std::weak_ptr<HttpJsonRpcClient> async_me;

	std::string buffer;
	std::vector<HttpClient::HeaderPair> headers;

	void parseResponse(json::Value id, HttpClientRequest &resp);
	void parseResponseAsync(json::Value id, std::unique_ptr<HttpClientRequest> &&resp);
	void finishResponse(const json::Value &response) noexcept;

	virtual void sendRequest(const json::Value &id, const json::Value &request);

};


}
