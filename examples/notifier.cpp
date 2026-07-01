#include <iostream>
#include <unistd.h>
#include "ubus.hpp"

std::unique_ptr<ubus> srv;

int main(int argc, char **argv) {

	srv = std::make_unique<ubus>();

	srv->add_object("sensor", {
		{ .name = "read", .cb = [](const auto&, const auto&, JSON& res) {
			res["temp"] = 23.5;
		}}
	}, [](bool active) {
		std::cout << "Subscribers: " << (active ? "yes" : "no") << "\n";
	});

	uloop::task::add([]() -> int {

		static int count = 0;
		srv->notify("sensor", "update", JSON({{"temp", 23.5 + count}, {"count", count++}}));

		return 2000;
	}, 2000);

	uloop::run();
	return 0;
}
