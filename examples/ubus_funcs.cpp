#include <iostream>

#include "json.hpp"
#include "ubus_funcs.hpp"

int ubus_get(const std::string& method, const std::string& msg, std::string& result) {

	JSON json;
	std::cout << "\ncall to ubus_get with method " << method << std::endl;

	if ( !msg.empty()) {

		try {
			json = JSON::parse(msg);
		} catch ( const JSON::exception& e ) {
			std::cout << "problem parsing:\n" << msg << "/nerror: " << e.what() << std::endl;
		}
	}

	if ( !json.empty())
		std::cout << "received args: " << json.dump(false) << std::endl;

	JSON answer;
	answer["ping"] = "pong";
	result = answer.dump(false);

	std::cout << "replying: " << answer.dump(false) << std::endl;

	return 0;
}

int ubus_list(const std::string& method, const std::string& msg, std::string& result) {

	std::cout << "\ncall to ubus_list with method " << method << std::endl;
	result = "{\"list\":[1,2,3,4,5]}";
	return 0;
}

int ubus_test(const std::string& method, const std::string& msg, std::string& result) {

	JSON json;
	std::cout << "\ncall to ubus_test with method " << method << std::endl;

	if ( !msg.empty()) {

		std::error_code err;

		try {
			json = JSON::parse(msg);
			std::cout << "received args: " << json.dump(false) << std::endl;

		} catch ( const JSON::exception& e ) {
			std::cout << "problem parsing:\n" << msg << "/nerror: " << e.what() << std::endl;
		}

	} else std::cout << "no arguments provided" << std::endl;

	result = "{\"test\":" + json.dump(false) + "}";

	return 0;
}

int ubus_test2(const std::string& method, const std::string& msg, std::string& result) {

	JSON json;
	json["result"] = true;
	result = json.dump(false);

	std::cout << "\ncall to ubus_test2 with method " << method << std::endl;

	return 0;
}
