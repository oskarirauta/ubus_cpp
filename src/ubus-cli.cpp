#include <ubus.hpp>

static blob_buf b;

ubus::client::client(std::string ubus_socket, int timeout) {

	this -> _sockfd = ubus_socket;
	this -> _timeout = timeout;
	this -> ctx = ubus_connect(this -> _sockfd.empty() ? NULL : this -> _sockfd.c_str());

	if ( !this -> ctx )
		throw ubus::exception("failed to connect to ubus socket " + this -> _sockfd, -1);
}

ubus::client::~client() {
	ubus_free(this -> ctx);
}

const std::string& ubus::client::sockfd() {
	return this -> _sockfd;
}

int ubus::client::timeout() {
	return this -> _timeout;
}

static void recv(struct ubus_request *req, int type, struct blob_attr *msg) {

	if ( !msg )
		return;

	std::string &s = *(static_cast<std::string*>(req -> priv));
        s = std::string(blobmsg_format_json(msg, true));
}

JSON ubus::client::call(const std::string& obj, const std::string& cmd, const JSON& args) {

	uint32_t id;

	if ( int ret = ubus_lookup_id(this -> ctx, obj.c_str(), &id); ret != 0 )
		throw ubus::exception("call to " + obj + " failed, cannot find object's id", -1);

	blob_buf_init(&b, 0);
	if ( !args.empty() && !blobmsg_add_json_from_string(&b, args.dump(false).c_str()))
		throw ubus::exception("failed to call " + obj + ", malformed json arguments", -1);

	std::string s;
	void *priv = static_cast<void*>(&s);

	if ( int ret = ubus_invoke(this -> ctx, id, cmd.c_str(), b.head, recv, priv, this -> _timeout * 1000); ret != 0 )
		throw ubus::exception("failed to call " + obj + " with command " + cmd + " with error " + std::string(ubus_strerror(ret)), ret);

	JSON j;
	try {
		j = JSON::parse(s);
	} catch (...) {
		j = JSON::Object();
	}

	return j;
}
