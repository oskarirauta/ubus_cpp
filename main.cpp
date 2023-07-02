#include <iostream>
#include <thread>

#include "ubus.hpp"
#include "ubus_funcs.hpp"

/*
static void die_handler(int signum) {

	if ( logger::output_level[logger::type::verbose] ||
		logger::output_level[logger::type::vverbose] ||
		logger::output_level[logger::type::debug] )
		logger::info << "received " << Signal::string(signum) << " signal" << std::endl;
	else logger::info << "received TERM signal" << std::endl;

	if ( !uloop_cancelled ) {
		logger::verbose << "stopping ubus service" << std::endl;
		uloop_end();
	}
}
*/

int main(const int argc, const char **argv) {

	std::cout << "ubus test" << std::endl;

	uloop_init();
	ubus::context *context;

	try {
		context = new ubus::context;
	} catch ( ubus::exception &e ) {
		std::cout << "error while creating ubus context:\n" << e.what() << std::endl;
		return e.code();
	}

	try {
		context -> add_object("ubus_test", {

			UBUS_HANDLER("test", ubus_test),
			UBUS_HANDLER("get", ubus_get),
			UBUS_HANDLER("list", ubus_list),
		});
	} catch ( ubus::exception &e ) {
		std::cout << "Error. Failed to add ubus object \"ubus_test\":\n " <<
				e.what() << " (code " << e.code() << ")" << std::endl;
		return e.code();
	}

	std::cout << "\ncreated new ubus object: ubus_test with methods test, get and list" << std::endl;

	std::cout << "starting ubus service" << std::endl;
	uloop_run();

	uloop_done();
	delete context;

	std::cout << "ubus service has stopped" << std::endl;
	std::cout << "exiting" << std::endl;

	return 0;
}
