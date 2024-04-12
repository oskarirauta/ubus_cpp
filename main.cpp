#include <iostream>
#include <filesystem>
#include "ubus.hpp"

ubus* srv;
int renew = 2000;

int hello_func(const std::string& method, const JSON& req, JSON& res) {

	res["hello"] = "world";
	std::cout << "\nexecuted function " << method << "\n" <<
			"sending back result:\n" << res << std::endl;
        return 0;
}

int exit_func(const std::string& method, const JSON& req, JSON& res) {

	res["exiting"] = "now";
	std::cout << "exiting uloop" << std::endl;
	uloop::exit();
	return 0;
}

int hinted_func(const std::string& method, const JSON& req, JSON& res) {

	if ( req.contains("value") && req["value"].convertible_to(JSON::STRING)) {
		res["value"] = req["value"];
		return 0;
	}

	if ( req.empty() || ( !req.contains("number" ) && !req.contains("string"))) {

		res["error"] = "hinted func needs either a number or string variable, or both";
		return 0;
	}

	int n;
	std::string s;
	bool number = false;
	bool string = false;

	if ( req.contains("number") && !req["number"].convertible_to(JSON::INT)) {

		res["error"] = "number variable is not convertible to numeric value";
		return 0;

	} else if ( req.contains("number")) {
		n = req["number"].to_int();
		number = true;
	}

	if ( req.contains("string") && !req["string"].convertible_to(JSON::STRING)) {

		res["error"] = "string variable is not convertible to text value";
		return 0;

	} else if ( req.contains("string")) {
		s = req["string"].to_string();
		string = true;
		if ( s.empty()) s = "(empty string)";
	}

	if ( string )
		res["string"] = s;

	if ( number )
		res["number"] = n;

	return 0;
}

int my_task() {

	if ( renew == 500 ) {

		std::cout << "executed my task last time, not renewing" << std::endl;
		return 0;
	}

	std::cout << "executed my task, firing next time in " << renew - 500 << "ms" << std::endl;
	renew -= 500;
	return renew;

}

int call_task() {

	try {
		JSON j = srv -> call("system", "board");
		std::cout << "ubus call to system with command board:\n" << j << std::endl;
	} catch ( const ubus::exception& e ) {
		std::cout << "failed to perform ubus call to system with command board, reason:\n" << e.what() << std::endl;
	}

	return 0;
}

int server_main() {

	std::cout << "ubus server test\n" << std::endl;

	uloop::task::add(my_task, 1000);
	uloop::task::add(call_task, 1250);

	try {
		srv -> add_object("ubus_test", {
			{ .name = "hello", .cb = hello_func },
			{ .name = "hello1", .cb = hello_func },
			{ .name = "hello2", .cb = hello_func },
			{ .name = "hello3", .cb = hello_func },
			{ .name = "hinted", .cb = hinted_func, .hints = {{ "number", JSON::TYPE::INT }, { "string", JSON::TYPE::STRING }}},
			{ .name = "hinted2", .cb = hinted_func, .hints = {{ "value", JSON::TYPE::STRING }}},
			{ .name = "exit", .cb = exit_func }
		});

		srv -> add_object("ubus_test2", {
			{ .name = "hello", .cb = hello_func },
			{ .name = "no_cb" }
		});

	} catch ( const ubus::exception& e ) {
		std::cout << "failed to add ubus object, reason:\n" << e.what() << std::endl;
		return 1;
	}

	uloop::run();

	return 0;
}

int client_main() {

	std::cout << "ubus client test\n" << std::endl;

	try {
		JSON j = srv -> call("ubus_test", "hello");
		std::cout << "result of json call to ubus_test with command hello:\n" << j << std::endl;

	} catch ( const ubus::exception& e ) {

		std::cout << "failed to call ubus_test with command hello, reason:\n" <<
			e.what() << "\n\nAre you sure, you are running server while starting client test?" << std::endl;
		return 1;
	}

	return 0;
}

int main(int argc, char **argv) {

	try {
		srv = new ubus;
	} catch ( const ubus::exception& e ) {
		std::cout << "failed to create ubus context, reason:\n" << e.what() << std::endl;
		return 1;
	}

	int res;
	std::filesystem::path cmd(argv[0]);

	if ( cmd.filename() == "client" )
		res = client_main();
	else res = server_main();

	delete srv;
	std::cout << "exiting.." << std::endl;
	return res;
}
