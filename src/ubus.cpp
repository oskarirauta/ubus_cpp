#include "ubus.hpp"

struct blob_buf ubus::service::blob;
struct blob_buf ubus::client::blob;

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

/*
 *
 * Client side code begins here
 *
 */

ubus::client::client(std::string ubus_socket, int timeout) {

	this -> _sockfd = ubus_socket;
	this -> _timeout = timeout;
	this -> ctx = ubus_connect(this -> _sockfd == "" ? NULL : this -> _sockfd.c_str());

	if ( !this -> ctx )
		throw ubus::exception("failed to connect to ubus socket " + this -> _sockfd, -1);
	else ubus_add_uloop(this -> ctx);
}

ubus::client::~client() {
	ubus_free(this -> ctx);
}

void my_recv(struct ubus_request *req, int type, struct blob_attr *msg) {

	if ( !msg )
		return;

	std::string &s = *(static_cast<std::string*>(req -> priv));
	s = std::string(blobmsg_format_json(msg, true));
}

void ubus::client::call(const std::string &obj, const std::string &cmd, std::string &result, const std::string &args) {

	uint32_t id;

	if ( int ret = ubus_lookup_id(this -> ctx, obj.c_str(), &id); ret != 0 )
		throw ubus::exception("call to " + obj + " failed, cannot look up it's id.", -1);

	blob_buf_init(&ubus::client::blob, 0);
	if ( !args.empty() && !blobmsg_add_json_from_string(&ubus::client::blob, args.c_str()))
		throw ubus::exception("cannot call " + obj + ", malformed json arguments", -1);

	void *priv = static_cast<void*>(&result);

	if ( int ret = ubus_invoke(this -> ctx, id, cmd.c_str(), ubus::client::blob.head, my_recv, priv, this -> _timeout * 1000); ret != 0 )
                throw ubus::exception("call to " + obj + " with command " + cmd + " failed on invoke with " + std::string(ubus_strerror(ret)), ret);
}
