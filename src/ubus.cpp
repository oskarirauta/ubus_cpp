extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#include "ubus.hpp"

static bool blob_buf_initialized_once = false;
static std::map<std::pair<std::string, std::string>, std::function<void(const std::string&, const JSON&, JSON&)>> ubus_methods;
static blob_buf b;
static ubus* ubus_singleton;

struct ubus::Context {

	int timeout;
	ubus_context *ctx;
	const std::string socket;

	Context(const std::string&, int);
	virtual ~Context();
};

struct ubus::Object {

	std::unique_ptr<ubus_object_type> type;
	std::unique_ptr<ubus_object> object;
	std::vector<ubus_method> methods;
	std::map<std::string, std::vector<blobmsg_policy>> policy;

	Object(const std::string& name, const std::vector<ubus::method>& methods);
};

static int common_handler(ubus_context *ctx, ubus_object *obj, ubus_request_data *req, const char *method, blob_attr *msg) {

	JSON _req, _res;

	if ( blob_len(msg) > 0 ) {

		std::string _msg = std::string(blobmsg_format_json(msg, true));

		if ( !_msg.empty()) {

			try {
				_req = JSON::parse(_msg);

			} catch (...) {
				_req = JSON::Object();
			}
		}
	}

	int code = 0;
	std::string obj_name(obj -> name);
	std::string method_name(method);

	if ( auto it = ubus_methods.find(std::make_pair(obj_name, method_name)); it != ubus_methods.end())
		if ( it -> second )
			it -> second(method_name, _req, _res);

	if ( _res.empty())
		_res = JSON::Object();

	blob_buf_init(&b, 0);
	blobmsg_add_json_from_string(&b, _res.dump(false).c_str());
	ubus_send_reply(ctx, req, b.head);

        return code;

}

ubus::Context::Context(const std::string& socket, int timeout) : timeout(timeout), socket(socket) {

	this -> ctx = this -> socket.empty() ? ubus_connect(nullptr) : ubus_connect(socket.c_str());
	ubus_add_uloop(this -> ctx);

	if ( !blob_buf_initialized_once ) {
		blob_buf_initialized_once = true;
		memset(&b, 0, sizeof(b));
	}
}

ubus::Context::~Context() {

	ubus_free(this -> ctx);
	blob_buf_free(&b);
}

static const auto json_type_to_blobmsg_type(const JSON::TYPE& t) {

	switch ( t ) {
		case JSON::TYPE::OBJECT: return BLOBMSG_TYPE_TABLE;
		case JSON::TYPE::ARRAY: return BLOBMSG_TYPE_ARRAY;
		case JSON::TYPE::STRING: return BLOBMSG_TYPE_STRING;
		case JSON::TYPE::FLOAT: return BLOBMSG_TYPE_DOUBLE;
		case JSON::TYPE::INT: return BLOBMSG_TYPE_INT32;
		case JSON::TYPE::BOOL: return BLOBMSG_TYPE_BOOL;
		default: return BLOBMSG_TYPE_UNSPEC;
	}
}

ubus::Object::Object(const std::string& name, const std::vector<ubus::method>& methods) {

	for ( const auto& method : methods ) {

		this -> methods.push_back(ubus_method());
		this -> methods.back().name = method.name.c_str();
		this -> methods.back().handler = common_handler;
		this -> methods.back().tags = 0;

		if ( !method.hints.empty()) {

			this -> policy.emplace(method.name, std::vector<blobmsg_policy>());

			for ( const std::pair<std::string, JSON::TYPE>& p: method.hints )
				this -> policy[method.name].push_back({ .name = p.first.c_str(), .type = json_type_to_blobmsg_type(p.second) });

			this -> methods.back().policy = this -> policy[method.name].data();
			this -> methods.back().n_policy = this -> policy[method.name].size();
		}

		ubus_methods.emplace(std::make_pair(name, method.name), method.cb);
	}

	this -> type = std::unique_ptr<ubus_object_type>(new ubus_object_type);
	this -> type -> name = name.c_str();
	this -> type -> id = 0;
	this -> type -> methods = &this -> methods[0];
	this -> type -> n_methods = (int)this -> methods.size();

	this -> object = std::unique_ptr<ubus_object>(new ubus_object);
	this -> object -> name = name.c_str();
	this -> object -> type = this -> type.get();
	this -> object -> methods = &this -> methods[0];
	this -> object -> n_methods = (int)this -> methods.size();
}

ubus::ubus(const std::string& socket, int timeout) {

	if ( ubus_singleton != nullptr )
		throw ubus::exception("ubus instance already created, multiple instances are not supported", -1);

	uloop_init();

	this -> context = std::unique_ptr<Context>(new Context(socket, timeout));
	ubus_singleton = this;
}

ubus::~ubus() {

	ubus_singleton = nullptr;
	auto& deleter = this -> context.get_deleter();
	auto* released = this -> context.release();
	deleter(released);
}

void ubus::add_object(const std::string& name, const std::vector<ubus::method>& methods) {

	if ( methods.empty())
		throw ubus::exception("no methods defined for object " + name, -1);

	for ( const auto& method : methods )
		if ( method.name.empty())
			throw ubus::exception("empty method names not allowed", -1);

	this -> objects.push_back(std::unique_ptr<ubus::Object>(new ubus::Object(name, methods)));

	if ( int ret = ubus_add_object(this -> context -> ctx, this -> objects.back() -> object.get()); ret != 0 ) {

		std::string err(ubus_strerror(ret));
		throw ubus::exception(err.empty() ? ( "Failed to add ubus object " + name ) : err, ret);
	}
}

static void recv(ubus_request* req, int type, blob_attr* msg) {

	if ( !msg )
		return;

	char *s = blobmsg_format_json(msg, true);
	*(std::string*)(req -> priv) = std::string(s);
	free(s);
}

JSON ubus::call(const std::string& obj, const std::string& cmd, const JSON& args) const {

	uint32_t id;

	if ( int ret = ubus_lookup_id(this -> context -> ctx, obj.c_str(), &id); ret != 0 )
		throw ubus::exception("call to " + obj + " failed, cannot find object's id", -1);

	blob_buf_init(&b, 0);

	if ( !args.empty() && !blobmsg_add_json_from_string(&b, args.dump(false).c_str()))
		throw ubus::exception("failed to call " + obj + ", malformed json arguments", -1);

	std::string s;

	if ( int ret = ubus_invoke(this -> context -> ctx, id, cmd.c_str(), b.head,
			recv, &s, this -> context -> timeout * 1000); ret != 0 ) {

		std::string err(ubus_strerror(ret));
		throw ubus::exception(err.empty() ? ( "failed to call " + obj + " with command " + cmd ) : err, ret);
	}

	JSON j;

	try {
		j = JSON::parse(s);
	} catch (...) {
		j = JSON::Object();
	}

	return j;
}

std::string ubus::socket() const {

	return this -> context -> socket;
}

int ubus::timeout() const {

	return this -> context -> timeout;
}

int ubus::exception::code() const {
	return _code;
}

const char *ubus::exception::what() const noexcept {
	return this -> _msg.c_str();
}
