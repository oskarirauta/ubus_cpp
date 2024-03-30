#include <iostream>
#include <filesystem>
#include "ubus.hpp"

int hello_func(const std::string& s, const JSON& req, JSON& res) {

        std::cout << "executed function " << s << std::endl;
	res["hello"] = "world";
	std::cout << "sending back result:\n" << res << std::endl;
        return 0;
}

int server_main() {

	std::cout << "ubus server test" << std::endl;

	uloop_init();
	ubus::service *srv;

	try {
		srv = new ubus::service;
	} catch ( const ubus::exception& e ) {
		std::cout << "failed to construct new ubus service, reason:\n" << e.what() << std::endl;
		return 1;
	}

	try {
		srv -> add_object("ubus_test", {
				{ .name = "hello", .cb = hello_func },
				{ .name = "foo", .cb = [](const std::string& s, const JSON& req, JSON& res) { res["foo"] = "bar"; return 0;  }}
		});
	} catch ( const ubus::exception& e ) {
		std::cout << "failed to add object 'ubus_test' with methods hello and foo, reason:\n" << e.what() << std::endl;
	}

	uloop_run();
	uloop_done();
	delete srv;

	return 0;
}

int client_main() {

	std::cout << "ubus client test\n" << std::endl;

	//uloop_init();
	ubus::client *cli;

	try {
		cli  = new ubus::client;
	} catch ( const ubus::exception& e ) {
		std::cout << "failed to construct ubus::client, reason:\n" << e.what() << std::endl;
		return 1;
	}

	try {
		JSON j = cli -> call("ubus_test", "hello");
		std::cout << "result of json call to ubus_test with command hello:\n" << j << std::endl;
	} catch ( const ubus::exception& e ) {
		std::cout << "failed to call ubus_test with command hello, reason:\n" <<
			e.what() << "\n\nAre you sure, you are running server while starting client test?" << std::endl;
		return 1;
	}

	return 0;
}

int main(int argc, char **argv) {

	std::filesystem::path cmd(argv[0]);

	if ( cmd.filename() == "client" )
		return client_main();
	else return server_main();
}
