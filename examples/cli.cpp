#include <iostream>

#include "json.hpp"
#include "ubus.hpp"

void make_call(ubus::client *client, const std::string& obj, const std::string& cmd, const std::string& args) {

	std::string result;

	try {
		client -> call(obj, cmd, result, args);

		try {
			JSON json = JSON::parse(result);
			std::cout << "ubus call to " << obj << " with command " << cmd << " - received:\n" << json.dump(false) << "\n" << std::endl;

		} catch ( const JSON::exception& e ) {
			std::cout << "making ubus call to " << obj << " with command: " << cmd << std::endl;
			std::cout << "failed to parse json:\n" << result << "\nerror: " << e.what() << std::endl;
			return;
		}

	} catch ( const ubus::exception& e ) {
		std::cout << "ubus call to " << obj << " with command " << cmd << " failed with error:\n" <<
			"error: " << e.code() << ", " << e.what() << "\n" << std::endl;
	}
}

int main(const int argc, const char **argv) {

	std::cout << "ubus client test" << std::endl;
	std::cout << "Make sure that srv example is running in the background\n" << std::endl;

	ubus::client *client;
	try {
		client = new ubus::client;
	} catch ( ubus::exception &e ) {
		std::cout << "error while creating ubus connection:\n" << e.what() << std::endl;
		return e.code();
	}

	make_call(client, "ubus_test2", "list", "");
	make_call(client, "ubus_test", "list", "");
	make_call(client, "ubus_test", "get", "{\"hello\":\"world\",\"x\":1,\"y\":2,\"z\":[1,2,3,4,5]}");
	make_call(client, "ubus_test", "test", "{\"hello\":\"world\",\"x\":1,\"y\":2,\"z\":[1,2,3,4,5]}");
	delete client;
	return 0;
}
