#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
#include <ostream>

extern "C" {
#include <libubox/blobmsg_json.h>
#include <libubus.h>
}

#include "json.hpp"

namespace ubus {

	struct METHOD {

		std::string name;
		std::function<int(const std::string&, const JSON&, JSON&)> cb;
		std::vector<std::pair<std::string, JSON::TYPE>> hints;
        };

	class client {

		private:
			std::string _sockfd;
			ubus_context *ctx;
			int _timeout;

		public:

			const std::string& sockfd();
			int timeout();
			JSON call(const std::string &obj, const std::string& cmd, const JSON& args = JSON());

			client(std::string ubus_socket = "", int timeout = 30);
			~client();

	};

	class service {

		private:

			std::string _sockfd;
			ubus_context *ctx;
			std::vector<ubus_object_type> types;
			std::vector<ubus_object> objects;
			std::vector<std::vector<ubus_method>> methods;

		public:

			const std::string& sockfd();

			void add_object(const std::string& name, const std::vector<ubus::METHOD>& methods);
			void add_objects(const std::map<std::string, std::vector<ubus::METHOD>>& objects);

			service(const std::string& ubus_socket = "");
			~service();
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
