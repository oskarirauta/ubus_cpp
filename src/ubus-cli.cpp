#include <list>
#include <chrono>
#include <thread>

#include "ubus-cli.hpp"

struct blob_buf ubus::client::blob;

struct ubus_msg_receiver {
	ubus_context *ctx;
	std::chrono::milliseconds expires;
	std::string msg;
};

static std::list<ubus_msg_receiver> receivers;
static std::mutex receivers_mutex;

static std::chrono::milliseconds get_now(int advance) {
	std::chrono::milliseconds _now = std::chrono::duration_cast<std::chrono::milliseconds>(
		std::chrono::system_clock::now().time_since_epoch());
	return advance == 0 ? _now : _now + std::chrono::milliseconds(advance);
}

static void make_receiver(ubus_context *ctx, int timeout) {

	std::lock_guard<std::mutex> guard(receivers_mutex);

	auto now = get_now(0);
	receivers.remove_if([&now](const ubus_msg_receiver& entry) { return now > entry.expires; });
	receivers.push_back(ubus_msg_receiver(ctx, get_now(timeout + 500)));
}

static bool waiting_receiver_msg(ubus_context *ctx) {

	std::lock_guard<std::mutex> guard(receivers_mutex);

	auto now = get_now(0);
	receivers.remove_if([&now](const ubus_msg_receiver& entry) { return now > entry.expires; });
	if ( auto it = std::find_if(receivers.begin(), receivers.end(), [&ctx](const ubus_msg_receiver& entry) {
		return entry.ctx == ctx && entry.msg.empty(); }); it != receivers.end()) return true;
	return false;
}

static bool get_receiver_msg(ubus_context *ctx, std::string &result) {

	std::lock_guard<std::mutex> guard(receivers_mutex);
	bool ret = false;

	if ( auto it = std::find_if(receivers.begin(), receivers.end(), [&ctx](const ubus_msg_receiver& entry) {
		return entry.ctx == ctx && !entry.msg.empty(); }); it != receivers.end()) {
		result = it -> msg;
		ret = true;
	}

	if ( ret )
		receivers.remove_if([&ctx](const ubus_msg_receiver& entry){ return entry.ctx == ctx; });

	auto now = get_now(0);
	receivers.remove_if([&now](const ubus_msg_receiver& entry) { return now > entry.expires; });

	return ret;
}

void recv_receiver_msg(struct ubus_request *req, int type, struct blob_attr *msg) {

	std::lock_guard<std::mutex> guard(receivers_mutex);

	auto now = get_now(0);
	receivers.remove_if([&now](const ubus_msg_receiver& entry) { return now > entry.expires; });

	if ( !msg ) return;
	if ( auto it = std::find_if(receivers.begin(), receivers.end(), [&req](const ubus_msg_receiver& entry) {
		return entry.ctx == req -> ctx; }); it != receivers.end()) {
		it -> msg = std::string(blobmsg_format_json(msg, true));
	}
}

ubus::client::client(std::string ubus_socket, int timeout) {

	this -> _sockfd = ubus_socket;
	this -> ctx = ubus_connect(this -> _sockfd == "" ? NULL : this -> _sockfd.c_str());

	if ( !this -> ctx )
		throw ubus::exception("failed to connect to ubus socket " + this -> _sockfd, -1);
	else ubus_add_uloop(this -> ctx);
}

ubus::client::~client() {
	ubus_free(this -> ctx);
}

void ubus::client::call(const std::string &obj, const std::string &cmd, std::string &result, const std::string &args) {

	std::lock_guard<std::mutex> guard(this -> m);

	uint32_t id;

	if ( int ret = ubus_lookup_id(this -> ctx, obj.c_str(), &id); ret != 0 )
		throw ubus::exception("call to " + obj + " failed, cannot look up it's id.", -1);

	blob_buf_init(&ubus::client::blob, 0);
	if ( !args.empty() && !blobmsg_add_json_from_string(&ubus::client::blob, args.c_str()))
		throw ubus::exception("cannot call " + obj + ", malformed json arguments", -1);

	make_receiver(this -> ctx, this -> _timeout);
	if ( int ret = ubus_invoke(this -> ctx, id, cmd.c_str(), ubus::client::blob.head, recv_receiver_msg, NULL, this -> _timeout * 1000); ret != 0 )
		throw ubus::exception("call to " + obj + " with command " + cmd + " failed on invoke with " + std::string(ubus_strerror(ret)), ret);

	while ( waiting_receiver_msg(this -> ctx))
		std::this_thread::sleep_for(std::chrono::milliseconds(250));

	if ( !get_receiver_msg(this -> ctx, result))
		throw ubus::exception("call to " + obj + " with command " + cmd + " failed to receive reply", -1);
}
