#include "ubus.hpp"

struct blob_buf ubus::service::blob;

ubus::service::service(std::string ubus_socket) {

	this -> _sockfd = ubus_socket;
	this -> ctx = ubus_connect(this -> _sockfd == "" ? NULL : this -> _sockfd.c_str());

	if ( !this -> ctx )
		throw ubus::exception("failed to connect to ubus socket " + this -> _sockfd, -1);

	else ubus_add_uloop(this -> ctx);
}

ubus::service::~service() {
	ubus_free(this -> ctx);
}

void ubus::service::add_object(const std::string name, const std::vector<ubus_method> &methods) {

	this -> methods.push_back(methods);

	this -> t = (ubus_object_type){
		.name = name.c_str(),
		.id = this -> next_id,
		.methods = &this -> methods.back()[0],
		.n_methods = (int)this -> methods.back().size()
	};

	this -> o = {
		.name = name.c_str(),
		.type = &this -> t,
		.methods = &this -> methods.back()[0],
		.n_methods = (int)this -> methods.back().size()
	};

	if ( int ret = ubus_add_object(this -> ctx, &this -> o); ret != 0 )
		throw ubus::exception(ubus_strerror(ret), ret);
	else this -> next_id++;

}
