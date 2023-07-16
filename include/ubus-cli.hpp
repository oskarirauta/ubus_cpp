#pragma once

#include <vector>
#include <mutex>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#include "ubus-common.hpp"

namespace ubus {

	class client : public ubus::context {

		private:

			int _timeout;
			std::mutex m;

		public:

			static struct blob_buf blob;

			client(std::string ubus_socket = "", int timeout = 30);
			~client();

			void call(const std::string &obj, const std::string &cmd, std::string &result, const std::string &args = "");

			inline int timeout() {
				return this -> _timeout;
			}

	};

}
