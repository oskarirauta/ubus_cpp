#pragma once

#include <vector>
#include <string>
#include <exception>
#include <functional>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#define UBUS_HANDLER(name, func) ubus_method(name, [](ubus_context* ctx, ubus_object* obj, ubus_request_data* req, const char* method, blob_attr* msg){ \
	std::string _msg, _ret; \
	\
	if ( blob_len(msg) > 0 ) \
		_msg = std::string(blobmsg_format_json(msg, true)); \
	\
	int code = func(std::string(method), _msg, _ret); \
	\
	if ( _ret.size() != 0 ) { \
		blob_buf_init(&ubus::b, 0); \
		blobmsg_add_json_from_string(&ubus::b, _ret.c_str()); \
		ubus_send_reply(ctx, req, ubus::b.head); \
	} \
	\
	return code; \
	})

namespace ubus {

	extern struct blob_buf b;

	class exception : public std::exception {

		private:

			const char *message;
			const int _code;

		public:

			exception(const std::string& message, const int code) : message(message.c_str()), _code(code) {}

			const char *what() {
				return message;
			}

			int code() {
				return _code;
			}

	};

	class context {

		private:

			std::string _sockfd;

			ubus_object_type t;
			ubus_object o;
			std::vector<ubus_object_type> types;
			std::vector<ubus_object> objects;
			std::vector<std::vector<ubus_method> > methods;

			uint32_t next_id = 0;

		public:

			struct ubus_context *ctx;

			inline const std::string& sockfd() {
				return this -> _sockfd;
			}

			context(std::string ubus_socket = "");
			~context();

			void add_object(const std::string name, const std::vector<ubus_method> &methods);
	};

}
