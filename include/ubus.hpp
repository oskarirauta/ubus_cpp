#pragma once

#include <string>
#include <vector>
#include <map>
#include <functional>
#include <stdexcept>
#include <ostream>
#include <memory>

#include "json.hpp"
#include "uloop.hpp"

class ubus {

	private:
		struct Context;
		struct Object;

		std::vector<std::unique_ptr<Object>> objects;
		std::unique_ptr<Context> context;

	public:

		struct method {

			std::string name;
			std::function<void(const std::string&, const JSON&, JSON&)> cb;
			std::vector<std::pair<std::string, JSON::TYPE>> hints;
		};

		class exception : public std::runtime_error {

			private:
				const int _code;
				const std::string _msg;

			public:
				int code() const;
				const char *what() const noexcept override;

				exception(const std::string& msg, const int code) : std::runtime_error(""), _code(code), _msg(msg) {}
		};

		void add_object(const std::string& name, const std::vector<method>& methods);
		JSON call(const std::string& obj, const std::string& cmd, const JSON& args = JSON()) const;

		std::string socket() const;
		int timeout() const;

		ubus(const std::string& socket = "", int timeout = 30);
		virtual ~ubus();

};

inline std::ostream& operator <<(std::ostream& os, const ubus::exception& e) {
	os << e.what();
	return os;
}
