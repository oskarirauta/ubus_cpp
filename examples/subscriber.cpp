#include <iostream>
#include "ubus.hpp"

std::unique_ptr<ubus> srv;

int main(int argc, char **argv) {

	srv = std::make_unique<ubus>();

	auto sub = srv->subscribe("sensor", [](const std::string& type, const JSON& data) {
		std::cout << "Notify " << type << ": " << data << "\n";
	});

	std::cout << "Subscribed to sensor, waiting for notifications...\n";

	uloop::task::add([]() -> int {

		std::cout << "Exiting...\n";
		uloop::exit();

		return 0;
	}, 10000);

	uloop::run();
	return 0;
}
