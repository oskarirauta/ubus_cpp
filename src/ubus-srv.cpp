extern "C" {
#include <libubox/blobmsg.h>
}

#include "ubus.hpp"

static std::map<std::string, std::map<std::string, std::function<int(const std::string&, const JSON&, JSON&)>>> ubus_methods;
static blob_buf b;

int common_handler(struct ubus_context *ctx, struct ubus_object *obj,
                              struct ubus_request_data *req,
                              const char *method, struct blob_attr *msg) {

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
	std::string obj_name = obj -> name;
	std::string method_name = method;

	if ( auto it = ubus_methods.find(obj_name); it != ubus_methods.end())
		if ( auto it2 = ubus_methods[obj_name].find(method_name); it2 != ubus_methods[obj_name].end())
			code = ubus_methods[obj_name][method_name](method_name, _req, _res);

	if ( _res.empty())
		_res = JSON::Object();

	blob_buf_init(&b, 0);
	blobmsg_add_json_from_string(&b, _res.dump(false).c_str());
	ubus_send_reply(ctx, req, b.head);

        return code;
}

ubus::service::service(const std::string& ubus_socket) {

	this -> _sockfd = ubus_socket;
	this -> ctx = ubus_connect(this -> _sockfd == "" ? NULL : this -> _sockfd.c_str());

	if ( !this -> ctx )
		throw ubus::exception("failed to connect to ubus socket " + this -> _sockfd, -1);

	ubus_add_uloop(this -> ctx);
}

ubus::service::~service() {

	ubus_free(this -> ctx);
	ubus_methods.clear();
}

const std::string& ubus::service::sockfd() {

	return this -> _sockfd;
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

void ubus::service::add_object(const std::string& name, const std::vector<ubus::METHOD>& methods) {

	std::vector<ubus_method> _methods;
	std::vector<blobmsg_policy> policy;

	if ( methods.empty())
		throw ubus::exception("no methods defined for object " + name, 0);

	for ( const ubus::METHOD& m : methods ) {

		if ( m.name.empty())
			throw ubus::exception("empty method names not allowed", 0);

		ubus_method _m = {
			.name = m.name.c_str(),
			.handler = common_handler,
			.tags = 0
		};


		if ( m.cb )
			ubus_methods[name][m.name] = m.cb;

		if ( !m.hints.empty()) {

			for ( const std::pair<std::string, JSON::TYPE>& p : m.hints )
				policy.push_back({ .name = p.first.c_str(), .type = json_type_to_blobmsg_type(p.second) });

			_m.policy = policy.data();
			_m.n_policy = policy.size();

		}

		_methods.push_back(_m);
	}

	this -> methods.push_back(_methods);

	this -> types.push_back({
		.name = name.c_str(),
		.id = 0,
		.methods = this -> methods.back().data(),
		.n_methods = (int)this -> methods.back().size()
	});

	this -> objects.push_back({
		.name = name.c_str(),
		.type = &this -> types.back(),
		.methods = this -> methods.back().data(),
		.n_methods = (int)this -> methods.back().size()
	});

	ubus_add_object(this -> ctx, &this -> objects.back());
}

void ubus::service::add_objects(const std::map<std::string, std::vector<ubus::METHOD>>& objects) {

	for ( auto& o : objects ) {

		try {
			this -> add_object(o.first, o.second);
		} catch ( const ubus::exception& e ) {
			throw e;
		}
	}
}
