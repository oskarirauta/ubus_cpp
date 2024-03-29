#pragma once

#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
#include <ostream>
#include <string>
#include <bits/stdc++.h>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#include "json.hpp"

#define UBUS_HANDLER(n, f) ubus_method({ \
	.name = n, \
	.handler = [](ubus_context* ctx, ubus_object* obj, ubus_request_data* req, const char* method, blob_attr* msg) { \
		\
		static_assert(std::is_same_v<decltype(f), int(const std::string&, const JSON&, JSON&)>, \
			"function type does not match, function type must be int(const std::string&, const JSON&, JSON&)"); \
		\
		JSON _msg, _ret = JSON::Object(); \
		\
		if ( blob_len(msg) > 0 ) { \
			std::string _msg_str = std::string(blobmsg_format_json(msg, true)); \
			if ( !_msg_str.empty()) \
				try { \
					_msg = JSON::parse(_msg_str); \
				} catch (...) { \
					_msg = JSON::Object(); \
				} \
		} \
		\
		int code = f(std::string(method), _msg, _ret); \
		\
		if ( _ret.empty()) \
			_ret = JSON::Object(); \
		\
		blob_buf_init(&ubus::service::blob, 0); \
		blobmsg_add_json_from_string(&ubus::service::blob, _ret.dump(false).c_str()); \
		ubus_send_reply(ctx, req, ubus::service::blob.head); \
		\
		return code; \
	}, \
	.tags = 0 \
})

namespace ubus {

	class context {

		protected:

			std::string _sockfd;
			struct ubus_context *ctx;

		public:

			inline const std::string& sockfd() {
				return this -> _sockfd;
			}
	};

	class client : public ubus::context {

		private:

			int _timeout;

		public:

			static struct blob_buf blob;

			client(std::string ubus_socket = "", int timeout = 30);
			~client();

			void call(const std::string &obj, const std::string &cmd, std::string &result, const std::string &args = "");

			inline int timeout() {
				return this -> _timeout;
			}
	};

	class service : public ubus::context {

		protected:

			std::vector<ubus_object_type> t;
			std::vector<ubus_object> o;
			std::vector<ubus_object_type> types;
			std::vector<ubus_object> objects;
			std::vector<std::vector<ubus_method> > methods;

		public:

			static struct blob_buf blob;

			service(std::string ubus_socket = "");
			~service();

			void add_object(const std::string& name, const std::vector<ubus_method>& methods);
			void add_objects(const std::map<const std::string, const std::vector<ubus_method>>& objects);
	};

	class exception : public std::runtime_error {

		private:

			const int _code;
			const std::string _msg;

		public:

			inline int code() const {
				return _code;
			}

			inline const char *what() const noexcept override {
				return this -> _msg.c_str();
			}

			exception(const std::string& msg, const int code) : std::runtime_error(""), _code(code), _msg(msg) {}
	};
}

inline std::ostream& operator <<(std::ostream& os, const ubus::exception& e) {
	os << e.what();
	return os;
}
