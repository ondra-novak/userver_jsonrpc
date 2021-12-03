cmake_minimum_required(VERSION 2.8) 

add_library (userver_jsonrpc 
	index.html.cpp
	rpc.js.cpp
	rpcServer.cpp
	rpcClient.cpp		
	styles.css.cpp
)
