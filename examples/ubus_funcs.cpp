#include <iostream>

#include "json.hpp"
#include "ubus_funcs.hpp"

int ubus_get(const std::string& method, const JSON& msg, JSON& result) {

	std::cout << "\ncall to ubus_get with method " << method << std::endl;

	if ( !msg.empty())
		std::cout << "received args: " << msg.dump(false) << std::endl;
	result["ping"] = "pong";
	std::cout << "replying: " << result.dump(false) << std::endl;
	return 0;
}

int ubus_list(const std::string& method, const JSON& msg, JSON& result) {

	std::cout << "\ncall to ubus_list with method " << method << std::endl;
	result = JSON::parse("{\"list\":[1,2,3,4,5]}");
	return 0;
}

int ubus_test(const std::string& method, const JSON& msg, JSON& result) {

	std::cout << "\ncall to ubus_test with method " << method << std::endl;

	if ( msg.empty())
		std::cout << "no arguments provided" << std::endl;

	result = "{\"test\":" + msg.dump(false) + "}";

	return 0;
}

int ubus_test2(const std::string& method, const JSON& msg, JSON& result) {

	result["result"] = true;
	std::cout << "\ncall to ubus_test2 with method " << method << std::endl;

	return 0;
}
