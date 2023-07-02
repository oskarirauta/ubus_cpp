//#include "constants.hpp"
#include "ubus.hpp"

#include <functional>

struct blob_buf ubus::b;

ubus::context::context(std::string ubus_socket) {

	this -> _sockfd = ubus_socket;
	this -> ctx = ubus_connect(this -> _sockfd == "" ? NULL : this -> _sockfd.c_str());

	if ( !this -> ctx )
		throw ubus::exception("failed to connect to ubus socket " + this -> _sockfd, -1);

	else ubus_add_uloop(this -> ctx);
}

ubus::context::~context() {
	ubus_free(this -> ctx);
}

void ubus::context::add_object(const std::string name, const std::vector<ubus_method> &methods) {

	this -> methods.push_back(methods);

	this -> t = (ubus_object_type){
		.name = name.c_str(),
		.id = this -> next_id,
		.methods = &this -> methods.back()[0],
		.n_methods = (int)this -> methods.back().size() //ARRAY_SIZE(methods)
	};

	this -> o = {
		.name = name.c_str(),
		.type = &this -> t,
		.methods = &this -> methods.back()[0],
		.n_methods = (int)this -> methods.back().size() //ARRAY_SIZE(methods)
	};

	if ( int ret = ubus_add_object(this -> ctx, &this -> o); ret != 0 )
		throw ubus::exception(ubus_strerror(ret), ret);

	else this -> next_id++;
}
