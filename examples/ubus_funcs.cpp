#include <iostream>

#include "json.hpp"
#include "ubus_funcs.hpp"

int ubus_get(const std::string& method, const std::string& msg, std::string& result) {
	std::cout << "call to ubus_get with method " << method << std::endl;

	json::JSON args;

	if ( !msg.empty()) {
		try {
		args = json::JSON::Load(msg);
		} catch (std::error_code e) {
			std::cout << "problem parsing " << msg << std::endl;
			//std::cout << "error: " << e.what() << std::endl;
		}
	}

	if ( args.size() != 0 )
		std::cout << "received args: " << args << std::endl;

	json::JSON answer;
	answer["ping"] = "pong";
	result = answer.dumpMinified();

	std::cout << "replying: " << answer << std::endl;

	return 0;
}

int ubus_list(const std::string& method, const std::string& msg, std::string& result) {
	std::cout << "call to ubus_list with method " << method << std::endl;
	result = "{\"list\":[1,2,3,4,5]}";
	return 0;
}

int ubus_test(const std::string& method, const std::string& msg, std::string& result) {

	std::cout << "call to ubus_test with method " << method << std::endl;

	json::JSON args;

	if ( !msg.empty()) {
		std::error_code err;
		args = json::JSON::Load(msg, err);
		if ( args = json::JSON::Load(msg, err); !err )
			std::cout << "received args: " << args << std::endl;
		else std::cout << "problem parsing " << msg << std::endl;
	} else std::cout << "no arguments provided" << std::endl;

	result = "{\"test\":" + args.dumpMinified() + "}";

	return 0;
}
