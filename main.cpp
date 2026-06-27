#include <iostream>
#include <filesystem>
#include <unistd.h>
#include "ubus.hpp"

ubus* srv;
int renew = 2000;

// --- synchronous method: fill res, it is sent when you return -----------------
int hello_func(const std::string& method, const JSON& req, JSON& res) {

	res["hello"] = "world";
	std::cout << "\nexecuted function " << method << "\nsending back result:\n" << res << std::endl;
	return 0;
}

// --- deferred method: reply later (here: after 500ms) without blocking uloop ---
void slow_func(const std::string& method, const JSON& req, ubus::request r) {

	std::cout << "\nexecuted deferred function " << method << ", replying in 500ms" << std::endl;

	// Copy the request handle into a one-shot task and reply when it fires.
	uloop::task::add([r]() -> int {
		JSON res;
		res["done"] = true;
		res["note"] = "this reply was deferred";
		r.reply(res);
		return 0;
	}, 500);
}

int exit_func(const std::string& method, const JSON& req, JSON& res) {

	res["exiting"] = "now";
	std::cout << "exiting uloop" << std::endl;
	uloop::exit();
	return 0;
}

// --- asynchronous call: query a service without blocking the loop -------------
int async_task() {

	srv -> call_async("system", "board", JSON(), [](const JSON& result, bool ok) {
		std::cout << "\nasync call to system.board (ok=" << ( ok ? "true" : "false" ) << "):\n"
		          << result << std::endl;
	});
	return 0;   // one-shot
}

int event_task() {

	JSON data;
	data["pid"] = (long long)getpid();
	srv -> send_event("test.started", data);
	std::cout << "\nsent event test.started" << std::endl;
	return 0;   // one-shot
}

int server_main() {

	std::cout << "ubus server test\n" << std::endl;

	// listen for any event under "test."
	srv -> add_event_handler("test.*", [](const std::string& id, const JSON& data) {
		std::cout << "\nreceived event " << id << ":\n" << data << std::endl;
	});

	try {
		srv -> add_object("ubus_test", {
			{ .name = "hello", .cb = hello_func },
			{ .name = "slow",  .dcb = slow_func },                       // deferred reply
			{ .name = "hinted", .cb = hello_func,
			  .hints = {{ "number", JSON::TYPE::INT }, { "string", JSON::TYPE::STRING }}},
			{ .name = "exit",  .cb = exit_func }
		});

	} catch ( const ubus::exception& e ) {
		std::cout << "failed to add ubus object, reason:\n" << e.what() << std::endl;
		return 1;
	}

	// fire an async call and emit an event shortly after startup
	uloop::task::add(async_task, 500);
	uloop::task::add(event_task, 800);

	uloop::run();
	return 0;
}

int client_main() {

	std::cout << "ubus client test\n" << std::endl;

	try {
		JSON j = srv -> call("ubus_test", "hello");
		std::cout << "result of call to ubus_test.hello:\n" << j << std::endl;

	} catch ( const ubus::exception& e ) {
		std::cout << "failed to call ubus_test.hello, reason:\n" << e.what()
		          << "\n\nIs the server running in another session?" << std::endl;
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
