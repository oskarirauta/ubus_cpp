#include <iostream>

#include "json.hpp"
#include "ubus_funcs.hpp"

int ubus_get(const std::string& method, const std::string& msg, std::string& result) {
	std::cout << "called ubus_get with method " << method << std::endl;

	json::JSON json_msg;

	if ( !msg.empty()) {
		try {
		json_msg = json::JSON::Load(msg);
		} catch (std::error_code e) {
			std::cout << "problem parsing " << msg << std::endl;
			//std::cout << "error: " << e.what() << std::endl;
		}
		std::cout << "received message: " << msg << std::endl;
	}

	if ( json_msg.size() != 0 )
		std::cout << "received json: " << json_msg << std::endl;

	json::JSON answer;
	answer["ping"] = "pong";
	result = answer.dumpMinified();

	std::cout << "replying: " << answer << std::endl;

	return 0;
}

int ubus_list(const std::string& method, const std::string& msg, std::string& result) {
	std::cout << "called ubus_list with method " << method << std::endl;
	return 0;
}

int ubus_test(const std::string& method, const std::string& msg, std::string& result) {

	std::cout << "called ubus_test with method " << method << std::endl;

	if ( !msg.empty()) {
		std::error_code err;
		json::JSON json_msg = json::JSON::Load(msg, err);
		if ( !err )
			std::cout << "parsed: " << json_msg << std::endl;
	}

	return 0;
}
