#include "ubus-cli.hpp"

struct blob_buf ubus::client::blob;

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
