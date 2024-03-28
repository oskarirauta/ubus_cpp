#include <iostream>

#include "ubus.hpp"
#include "ubus_funcs.hpp"

int main(const int argc, const char **argv) {

	std::cout << "ubus server test" << std::endl;

	uloop_init();
	ubus::service *srv;

	try {
		srv = new ubus::service;
	} catch ( ubus::exception &e ) {
		std::cout << "error while creating ubus context:\n" << e.what() << std::endl;
		return e.code();
	}
/*
	// independent version
	try {
		srv -> add_object("ubus_test", {

			UBUS_HANDLER("test", ubus_test),
			UBUS_HANDLER("get", ubus_get),
			UBUS_HANDLER("list", ubus_list)
		});
	} catch ( ubus::exception &e ) {
		std::cout << "Error. Failed to add ubus object \"ubus_test\":\n " <<
				e.what() << " (code " << e.code() << ")" << std::endl;
		delete srv;
		return e.code();
	}

	try {
		srv -> add_object("ubus_test2", {
			UBUS_HANDLER("test", ubus_test2)
		});
	} catch ( ubus::exception &e ) {
		std::cout << "Error. Failed to add ubus object \"ubus_test2\":\n " <<
				e.what() << " (code " << e.code() << ")" << std::endl;
		delete srv;
		return e.code();
	}
*/

	// batch mode
	try {
		srv -> add_objects({{
			"ubus_test", {
				UBUS_HANDLER("test", ubus_test),
				UBUS_HANDLER("get", ubus_get),
				UBUS_HANDLER("list", ubus_list)
			}}, {
			"ubus_test2", {
				UBUS_HANDLER("test", ubus_test2)
			}}
		});
	} catch ( ubus::exception &e ) {
		std::cout << "Error. Failed to add ubus object \"ubus_test2\":\n " <<
			 e.what() << " (code " << e.code() << ")" << std::endl;
		delete srv;
		return e.code();
	}


	std::cout << "\ncreated new ubus object: ubus_test with methods test, get and list" << std::endl;
	std::cout << "created new ubus object: ubus_test2 with method test" << std::endl;

	std::cout << "starting ubus service" << std::endl;
	uloop_run();

	uloop_done();
	delete srv;

	std::cout << "ubus service has stopped" << std::endl;
	std::cout << "exiting" << std::endl;

	return 0;
}
