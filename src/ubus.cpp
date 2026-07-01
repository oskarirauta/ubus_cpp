#include <cstdlib>
#include <cstring>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#include "ubus.hpp"

static ubus* ubus_singleton = nullptr;

// (object, method) -> handlers. Stored globally so the C trampoline can find
// them without reaching into the ubus instance.
struct handler_entry {
	std::function<void(const std::string&, const JSON&, JSON&)> cb;
	std::function<void(const std::string&, const JSON&, ubus::request)> dcb;
};
static std::map<std::pair<std::string, std::string>, handler_entry> ubus_methods;

// event handler -> callback, so the C trampoline finds it without touching the
// (private) EventHandler type.
static std::map<ubus_event_handler*, std::function<void(const std::string&, const JSON&)>> ubus_event_cbs;

struct ubus::Context {

	int timeout;
	const std::string socket;
	bool auto_reconnect;
	ubus_context* ctx = nullptr;
	ubus_auto_conn autoconn;

	Context(const std::string&, int, bool);
	virtual ~Context();
};

struct ubus::Object {

	std::unique_ptr<ubus_object_type> type;
	std::unique_ptr<ubus_object> object;
	std::vector<ubus_method> methods;
	std::map<std::string, std::vector<blobmsg_policy>> policy;
	std::function<void(bool active)> subscribe_cb;

	Object(const std::string& name, const std::vector<ubus::method>& methods);
	virtual ~Object();
};

struct ubus::EventHandler {

	ubus_event_handler ev;
	std::string pattern;
};

struct ubus::request::state {

	ubus_context* ctx = nullptr;
	ubus_request_data req;
	bool replied = false;
};

struct ubus::subscription::state {
	ubus_context* ctx = nullptr;
	ubus_subscriber sub;
	uint32_t obj_id = 0;
	bool active = false;
	std::function<void(const std::string&, const JSON& data)> cb;
};

static std::map<ubus_subscriber*, std::function<void(const std::string&, const JSON&)>> ubus_subscriber_cbs;

// ----------------------------------------------------------- request handle ---
void ubus::request::reply(const JSON& res, int status) const {

	if ( !this -> _s || this -> _s -> replied )
		return;

	this -> _s -> replied = true;

	JSON r = res.empty() ? JSON::Object() : res;

	blob_buf b;
	memset(&b, 0, sizeof(b));
	blob_buf_init(&b, 0);
	if (!blobmsg_add_json_from_string(&b, r.dump(false).c_str())) {
		blob_buf_free(&b);
		return;
	}

	ubus_send_reply(this -> _s -> ctx, &this -> _s -> req, b.head);
	blob_buf_free(&b);

	ubus_complete_deferred_request(this -> _s -> ctx, &this -> _s -> req, status);
}

bool ubus::request::valid() const {
	return this -> _s && !this -> _s -> replied;
}

// --------------------------------------------------------- method dispatch ----
static int common_handler(ubus_context *ctx, ubus_object *obj, ubus_request_data *req, const char *method, blob_attr *msg) {

	JSON _req = JSON::Object();

	if ( msg && blob_len(msg) > 0 ) {

		char *s = blobmsg_format_json(msg, true);
		if ( s ) {
			try { _req = JSON::parse(std::string(s)); } catch ( ... ) { _req = JSON::Object(); }
			free(s);
		}
	}

	auto it = ubus_methods.find(std::make_pair(std::string(obj -> name), std::string(method)));
	if ( it == ubus_methods.end())
		return UBUS_STATUS_METHOD_NOT_FOUND;

	// deferred handler takes precedence: reply happens later via request::reply()
	if ( it -> second.dcb ) {

		auto st = std::make_shared<ubus::request::state>();
		st -> ctx = ctx;
		ubus_defer_request(ctx, req, &st -> req);
		it -> second.dcb(std::string(method), _req, ubus::request(st));
		return 0;
	}

	JSON _res = JSON::Object();
	if ( it -> second.cb )
		it -> second.cb(std::string(method), _req, _res);
	if ( _res.empty())
		_res = JSON::Object();

	blob_buf b;
	memset(&b, 0, sizeof(b));
	blob_buf_init(&b, 0);

	if (!blobmsg_add_json_from_string(&b, _res.dump(false).c_str())) {
		blob_buf_free(&b);
		return UBUS_STATUS_UNKNOWN_ERROR;
	}

	ubus_send_reply(ctx, req, b.head);
	blob_buf_free(&b);

	return 0;
}

// ----------------------------------------------------- subscriber changes ----
void ubus::subscriber_trampoline(void* ctx, void* obj) {

	ubus_context* uctx = (ubus_context*)ctx;
	ubus_object* uobj = (ubus_object*)obj;

	(void)uctx;
	if (!ubus_singleton)
		return;

	for ( auto& o : ubus_singleton -> objects )
		if ( o -> object.get() == uobj && o -> subscribe_cb )
			o -> subscribe_cb(!!uobj -> has_subscribers);
}

static int notify_trampoline(ubus_context *ctx, ubus_object *obj, ubus_request_data *req,
			      const char *method, blob_attr *msg) {
	(void)ctx;
	(void)obj;
	(void)req;
	(void)method;

	ubus_subscriber* sub = (ubus_subscriber*)((char*)obj - offsetof(ubus_subscriber, obj));

	auto it = ubus_subscriber_cbs.find(sub);
	if ( it == ubus_subscriber_cbs.end() || !it -> second )
		return 0;

	JSON data = JSON::Object();
	if ( msg && blob_len(msg) > 0 ) {
		char *s = blobmsg_format_json(msg, true);
		if ( s ) {
			try { data = JSON::parse(std::string(s)); } catch ( ... ) {}
			free(s);
		}
	}

	it -> second(std::string(method), data);
	return 0;
}

// --------------------------------------------------------------- subscribe ----
ubus::subscription::~subscription() {

	if ( !this -> _s || !this -> _s -> active )
		return;

	ubus_unsubscribe(this -> _s -> ctx, &this -> _s -> sub, this -> _s -> obj_id);
	ubus_subscriber_cbs.erase(&this -> _s -> sub);
	this -> _s -> active = false;
}

ubus::subscription ubus::subscribe(const std::string& obj_name,
				   std::function<void(const std::string& type, const JSON& data)> cb) {

	uint32_t id;
	if ( ubus_lookup_id(this -> context -> ctx, obj_name.c_str(), &id) != 0 )
		throw ubus::exception("subscribe failed, object not found: " + obj_name, -1);

	auto st = std::make_shared<subscription::state>();
	st -> ctx = this -> context -> ctx;
	st -> obj_id = id;
	st -> cb = cb;
	st -> active = true;

	memset(&st -> sub, 0, sizeof(st -> sub));
	st -> sub.cb = notify_trampoline;

	ubus_subscriber_cbs[&st -> sub] = cb;

	if ( ubus_register_subscriber(this -> context -> ctx, &st -> sub) != 0 ) {
		ubus_subscriber_cbs.erase(&st -> sub);
		throw ubus::exception("subscribe failed, cannot register subscriber", -1);
	}

	if ( ubus_subscribe(this -> context -> ctx, &st -> sub, id) != 0 ) {
		ubus_unregister_subscriber(this -> context -> ctx, &st -> sub);
		ubus_subscriber_cbs.erase(&st -> sub);
		throw ubus::exception("subscribe failed, cannot subscribe to " + obj_name, -1);
	}

	return subscription(st);
}

// ----------------------------------------------------------------- events -----
static void event_trampoline(ubus_context *ctx, ubus_event_handler *ev, const char *type, blob_attr *msg) {

	(void)ctx;
	auto it = ubus_event_cbs.find(ev);
	if ( it == ubus_event_cbs.end() || !it -> second )
		return;

	JSON data = JSON::Object();
	if ( msg && blob_len(msg) > 0 ) {
		char *s = blobmsg_format_json(msg, true);
		if ( s ) {
			try { data = JSON::parse(std::string(s)); } catch ( ... ) {}
			free(s);
		}
	}

	it -> second(std::string(type), data);
}

// ------------------------------------------------------------- (re)connect ----
static void ubus_connected_cb(ubus_context *ctx) {

	(void)ctx;
	if ( ubus_singleton ) {
		ubus_singleton -> register_objects();
		ubus_singleton -> register_events();
	}
}

ubus::Context::Context(const std::string& socket, int timeout, bool auto_reconnect)
	: timeout(timeout), socket(socket), auto_reconnect(auto_reconnect) {

	memset(&this -> autoconn, 0, sizeof(this -> autoconn));

	if ( auto_reconnect ) {

		this -> autoconn.path = this -> socket.empty() ? nullptr : this -> socket.c_str();
		this -> autoconn.cb = ubus_connected_cb;
		ubus_auto_connect(&this -> autoconn);
		this -> ctx = &this -> autoconn.ctx;

	} else {

		this -> ctx = this -> socket.empty() ? ubus_connect(nullptr) : ubus_connect(this -> socket.c_str());
		if ( !this -> ctx )
			throw ubus::exception("failed to connect to ubus", -1);
		ubus_add_uloop(this -> ctx);
	}
}

ubus::Context::~Context() {

	// TODO: should we also cancel pending requests/timeouts?

	if ( this -> auto_reconnect ) {
		uloop_timeout_cancel(&this -> autoconn.timer);
		ubus_shutdown(&this -> autoconn.ctx);
	} else if ( this -> ctx ) {
		ubus_free(this -> ctx);
	}
}

// ----------------------------------------------------------- type mapping -----
static auto json_type_to_blobmsg_type(const JSON::TYPE& t) {

	switch ( t ) {
		case JSON::TYPE::OBJECT: return BLOBMSG_TYPE_TABLE;
		case JSON::TYPE::ARRAY:  return BLOBMSG_TYPE_ARRAY;
		case JSON::TYPE::STRING: return BLOBMSG_TYPE_STRING;
		case JSON::TYPE::FLOAT:  return BLOBMSG_TYPE_DOUBLE;
		case JSON::TYPE::INT:    return BLOBMSG_TYPE_INT64;   // JSON ints are 64-bit
		case JSON::TYPE::BOOL:   return BLOBMSG_TYPE_BOOL;
		default:                 return BLOBMSG_TYPE_UNSPEC;
	}
}

ubus::Object::Object(const std::string& name, const std::vector<ubus::method>& methods) {

	for ( const auto& method : methods ) {

		this -> methods.push_back(ubus_method());
		this -> methods.back().name = strdup(method.name.c_str());
		this -> methods.back().handler = common_handler;
		this -> methods.back().tags = 0;

		if ( !method.hints.empty()) {

			this -> policy.emplace(method.name, std::vector<blobmsg_policy>());

			for ( const std::pair<std::string, JSON::TYPE>& p: method.hints )
				this -> policy[method.name].push_back({ .name = strdup(p.first.c_str()), .type = json_type_to_blobmsg_type(p.second) });

			this -> methods.back().policy = this -> policy[method.name].data();
			this -> methods.back().n_policy = this -> policy[method.name].size();
		}

		ubus_methods.emplace(std::make_pair(name, method.name), handler_entry { .cb = method.cb, .dcb = method.dcb });
	}

	this -> type = std::make_unique<ubus_object_type>();
	this -> type -> name = strdup(name.c_str());
	this -> type -> id = 0;
	this -> type -> methods = this -> methods.data();
	this -> type -> n_methods = (int)this -> methods.size();

	this -> object = std::make_unique<ubus_object>();
	memset(this -> object.get(), 0, sizeof(ubus_object));
	this -> object -> name = strdup(name.c_str());
	this -> object -> subscribe_cb = (ubus_state_handler_t)subscriber_trampoline;
	this -> object -> type = this -> type.get();
	this -> object -> methods = this -> methods.data();
	this -> object -> n_methods = (int)this -> methods.size();
}

ubus::Object::~Object() {

	free((void*)this -> object -> name);
	free((void*)this -> type -> name);

	for ( auto& m : this -> policy )
		for ( auto& p : m.second )
			free((void*)p.name);

	for ( auto& m : this -> methods )
		free((void*)m.name);
}

// ------------------------------------------------------------------ ubus ------
ubus::ubus(const std::string& socket, int timeout, bool auto_reconnect) {

	if ( ubus_singleton != nullptr )
		throw ubus::exception("ubus instance already created, multiple instances are not supported", -1);

	uloop::init();

	this -> context = std::make_unique<Context>(socket, timeout, auto_reconnect);
	ubus_singleton = this;
}

ubus::~ubus() {

	ubus_methods.clear();
	ubus_event_cbs.clear();
	ubus_singleton = nullptr;
	this -> context.reset();
}

void ubus::register_objects() {

	for ( auto& o : this -> objects )
		ubus_add_object(this -> context -> ctx, o -> object.get());
}

void ubus::register_events() {

	for ( auto& eh : this -> event_handlers )
		ubus_register_event_handler(this -> context -> ctx, &eh -> ev, eh -> pattern.c_str());
}

void ubus::add_object(const std::string& name, const std::vector<ubus::method>& methods,
		      std::function<void(bool active)> subscribe_cb) {

	if ( methods.empty())
		throw ubus::exception("no methods defined for object " + name, -1);

	for ( const auto& method : methods )
		if ( method.name.empty())
			throw ubus::exception("empty method names not allowed", -1);

	this -> objects.push_back(std::make_unique<ubus::Object>(name, methods));
	this -> objects.back() -> subscribe_cb = subscribe_cb;

	int ret = ubus_add_object(this -> context -> ctx, this -> objects.back() -> object.get());

	// In auto-reconnect mode a failure here usually just means "not connected
	// yet"; the connect callback will register it. Otherwise it is fatal.
	if ( ret != 0 && !this -> context -> auto_reconnect ) {
		std::string err(ubus_strerror(ret));
		throw ubus::exception(err.empty() ? ( "Failed to add ubus object " + name ) : err, ret);
	}
}

// ----------------------------------------------------------- synchronous call -
static void sync_recv(ubus_request* req, int type, blob_attr* msg) {

	(void)type;
	if ( !msg )
		return;

	char *s = blobmsg_format_json(msg, true);
	if ( s ) {
		*(std::string*)(req -> priv) = std::string(s);
		free(s);
	}
}

JSON ubus::call(const std::string& obj, const std::string& cmd, const JSON& args) const {

	uint32_t id;

	if ( int ret = ubus_lookup_id(this -> context -> ctx, obj.c_str(), &id); ret != 0 )
		throw ubus::exception("call to " + obj + " failed, cannot find object's id", -1);

	blob_buf b;
	memset(&b, 0, sizeof(b));
	blob_buf_init(&b, 0);

	if ( !args.empty() && !blobmsg_add_json_from_string(&b, args.dump(false).c_str())) {
		blob_buf_free(&b);
		throw ubus::exception("failed to call " + obj + ", malformed json arguments", -1);
	}

	std::string s;

	int ret = ubus_invoke(this -> context -> ctx, id, cmd.c_str(), b.head,
			sync_recv, &s, this -> context -> timeout * 1000);

	blob_buf_free(&b);

	if ( ret != 0 ) {
		std::string err(ubus_strerror(ret));
		throw ubus::exception(err.empty() ? ( "failed to call " + obj + " with command " + cmd ) : err, ret);
	}

	JSON j;
	try { j = JSON::parse(s); } catch ( ... ) { j = JSON::Object(); }
	return j;
}

// ---------------------------------------------------------- batch call ----
std::vector<JSON> ubus::call_batch(const std::vector<std::tuple<std::string, std::string, JSON>>& calls) const {

	std::vector<JSON> results;
	results.reserve(calls.size());

	for ( const auto& [obj, cmd, args] : calls )
		results.push_back(call(obj, cmd, args));

	return results;
}

// ---------------------------------------------------------- asynchronous call -
struct async_state {

	std::function<void(const JSON&, bool)> cb;
	std::string buf;
	bool got = false;
};

static void async_data_cb(ubus_request* req, int type, blob_attr* msg) {

	(void)type;
	if ( !msg )
		return;

	async_state* st = (async_state*)req -> priv;
	char *s = blobmsg_format_json(msg, true);
	if ( s ) {
		st -> buf = std::string(s);
		st -> got = true;
		free(s);
	}
}

static void async_complete_cb(ubus_request* req, int ret) {

	async_state* st = (async_state*)req -> priv;

	JSON result = JSON::Object();
	if ( st -> got ) {
		try { result = JSON::parse(st -> buf); } catch ( ... ) { result = JSON::Object(); }
	}

	if ( st -> cb )
		st -> cb(result, ret == 0);

	delete st;
	free(req);
}

void ubus::call_async(const std::string& obj, const std::string& cmd, const JSON& args,
		std::function<void(const JSON& result, bool ok)> cb) {

	uint32_t id;
	if ( ubus_lookup_id(this -> context -> ctx, obj.c_str(), &id) != 0 ) {
		if ( cb ) cb(JSON::Object(), false);
		return;
	}

	blob_buf b;
	memset(&b, 0, sizeof(b));
	blob_buf_init(&b, 0);
	if ( !args.empty()) {
		if ( !blobmsg_add_json_from_string(&b, args.dump(false).c_str())) {
			blob_buf_free(&b);
			if (cb) cb(JSON::Object(), false);
			return;
		}
	}

	ubus_request* req = (ubus_request*)calloc(1, sizeof(ubus_request));

	if ( int ret = ubus_invoke_async(this -> context -> ctx, id, cmd.c_str(), b.head, req); ret != 0 ) {
		blob_buf_free(&b);
		free(req);
		if (cb)
			cb(JSON::Object(), false);
		return;
	}

	async_state* st = new async_state();
	st -> cb = cb;

	req -> data_cb = async_data_cb;
	req -> complete_cb = async_complete_cb;
	req -> priv = st;

	ubus_complete_request_async(this -> context -> ctx, req);
	blob_buf_free(&b);
}

// ----------------------------------------------------------------- events -----
void ubus::send_event(const std::string& id, const JSON& data) const {

	blob_buf b;
	memset(&b, 0, sizeof(b));
	blob_buf_init(&b, 0);
	if ( !data.empty()) {
		if (!blobmsg_add_json_from_string(&b, data.dump(false).c_str())) {
			blob_buf_free(&b);
			return;
		}
	}

	ubus_send_event(this -> context -> ctx, id.c_str(), b.head);
	blob_buf_free(&b);
}

// --------------------------------------------------------------- notify -----
void ubus::notify(const std::string& obj_name, const std::string& type,
		  const JSON& data, int timeout) const {

	for ( auto& o : this -> objects ) {
		if ( std::string(o -> object -> name) != obj_name )
			continue;

		blob_buf b;
		memset(&b, 0, sizeof(b));
		blob_buf_init(&b, 0);

		if ( !data.empty() && !blobmsg_add_json_from_string(&b, data.dump(false).c_str())) {
			blob_buf_free(&b);
			return;
		}

		ubus_notify(this -> context -> ctx, o -> object.get(), type.c_str(), b.head, timeout);
		blob_buf_free(&b);
		return;
	}
}

void ubus::add_event_handler(const std::string& pattern,
		std::function<void(const std::string& id, const JSON& data)> cb) {

	auto eh = std::make_unique<EventHandler>();
	memset(&eh -> ev, 0, sizeof(eh -> ev));
	eh -> ev.cb = event_trampoline;
	eh -> pattern = pattern;

	ubus_event_cbs[&eh -> ev] = cb;

	ubus_register_event_handler(this -> context -> ctx, &eh -> ev, pattern.c_str());
	this -> event_handlers.push_back(std::move(eh));
}

// --------------------------------------------------------------- accessors ----
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
