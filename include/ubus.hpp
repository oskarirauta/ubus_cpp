#pragma once

#include <vector>
#include <functional>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#include "ubus-common.hpp"

#define UBUS_HANDLER(n, f) ubus_method({ \
        .name = n, \
        .handler = [](ubus_context* ctx, ubus_object* obj, ubus_request_data* req, const char* method, blob_attr* msg) { \
                \
                std::string _msg, _ret; \
                \
                if ( blob_len(msg) > 0 ) \
                        _msg = std::string(blobmsg_format_json(msg, true)); \
                \
                int code = f(std::string(method), _msg, _ret); \
                \
                if ( _ret.size() != 0 ) { \
                        blob_buf_init(&ubus::service::blob, 0); \
                        blobmsg_add_json_from_string(&ubus::service::blob, _ret.c_str()); \
                        ubus_send_reply(ctx, req, ubus::service::blob.head); \
                } \
                \
                return code; \
        }, \
        .tags = 0 \
})

namespace ubus {

	class service : public ubus::context {

		protected:

			ubus_object_type t;
			ubus_object o;
			std::vector<ubus_object_type> types;
			std::vector<ubus_object> objects;
			std::vector<std::vector<ubus_method> > methods;

			uint32_t next_id = 0;

		public:

			static struct blob_buf blob;

			service(std::string ubus_socket = "");
			~service();

			void add_object(const std::string name, const std::vector<ubus_method> &methods);
	};

}
