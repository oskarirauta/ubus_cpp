#pragma once

#include <string>
#include <exception>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

namespace ubus {

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

		protected:

			std::string _sockfd;
			struct ubus_context *ctx;

		public:

			inline const std::string& sockfd() {
				return this -> _sockfd;
			}

	};

}
