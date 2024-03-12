#pragma once

#include <string>
#include <stdexcept>
#include <ostream>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

namespace ubus {

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

inline std::ostream& operator <<(std::ostream& os, const ubus::exception& e) {

	os << e.what();
	return os;
}
