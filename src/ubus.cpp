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

void ubus::service::add_object(const std::string& name, const std::vector<ubus_method>& methods) {

	this -> methods.push_back(methods);

	this -> t.push_back((ubus_object_type) {
		.name = name.c_str(),
		.id = 0,
		.methods = &this -> methods.back()[0],
		.n_methods = (int)this -> methods.back().size()
	});

	this -> o.push_back((ubus_object) {
		.name = name.c_str(),
		.type = &this -> t.back(),
		.methods = &this -> methods.back()[0],
		.n_methods = (int)this -> methods.back().size()
	});

	if ( int ret = ubus_add_object(this -> ctx, &this -> o.back()); ret != 0 )
		throw ubus::exception(ubus_strerror(ret), ret);
}

void ubus::service::add_objects(const std::map<const std::string, const std::vector<ubus_method>>& objects) {

	for ( auto& obj : objects ) {

		this -> methods.push_back(obj.second);

		this -> t.push_back((ubus_object_type) {
			.name = obj.first.c_str(),
			.id = 0,
			.methods = &this -> methods.back()[0],
			.n_methods = (int)this -> methods.back().size()
		});

		this -> o.push_back((ubus_object) {
			.name = obj.first.c_str(),
			.type = &this -> t.back(),
			.methods = &this -> methods.back()[0],
			.n_methods = (int)this -> methods.back().size()
		});

		if ( int ret = ubus_add_object(this -> ctx, &this -> o.back()); ret != 0 )
			throw ubus::exception(ubus_strerror(ret), ret);
	}
}

