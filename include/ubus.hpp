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
		struct EventHandler;

		std::vector<std::unique_ptr<Object>> objects;
		std::vector<std::unique_ptr<EventHandler>> event_handlers;
		std::unique_ptr<Context> context;

	public:

		// Internal: (re-)register all objects and event handlers. Called
		// automatically on (re)connect; you normally never call these.
		void register_objects();
		void register_events();

		// Handle to a deferred ubus request. Given to a method's deferred
		// callback (method::dcb). Reply right away, or copy the handle into your
		// own state (it is shared, copyable) and reply later from a uloop task or
		// an async callback. Calling reply() more than once is a no-op.
		class request {

			public:
				struct state;   // opaque; defined in the implementation
				explicit request(const std::shared_ptr<state>& s) : _s(s) {}

				void reply(const JSON& res = JSON(), int status = 0) const;
				bool valid() const;

			private:
				std::shared_ptr<state> _s;
		};

		struct method {

			std::string name;

			// Synchronous handler: fill res; it is sent when you return.
			std::function<void(const std::string&, const JSON&, JSON&)> cb;

			// Deferred handler (optional). If set, it takes precedence over cb.
			// The reply is sent when you call req.reply(...) - now or later.
			std::function<void(const std::string&, const JSON&, request)> dcb;

			// Argument type hints (ubus method policy).
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

		// Register a ubus object with its methods.
		void add_object(const std::string& name, const std::vector<method>& methods);

		// Synchronous call: blocks until the reply arrives or the timeout
		// elapses; throws ubus::exception on failure.
		JSON call(const std::string& obj, const std::string& cmd, const JSON& args = JSON()) const;

		// Asynchronous call: returns immediately. cb(result, ok) fires from the
		// uloop when the reply arrives; ok is false on error/timeout (result empty).
		void call_async(const std::string& obj, const std::string& cmd, const JSON& args,
				std::function<void(const JSON& result, bool ok)> cb);

		// Broadcast an event on the bus (fire-and-forget).
		void send_event(const std::string& id, const JSON& data = JSON()) const;

		// Listen for events whose id matches pattern (a trailing '*' is a wildcard).
		// cb(id, data) fires from the uloop for every matching event.
		void add_event_handler(const std::string& pattern,
				std::function<void(const std::string& id, const JSON& data)> cb);

		std::string socket() const;
		int timeout() const;

		// auto_reconnect: survive a ubusd restart - reconnect and automatically
		// re-register all objects and event handlers (requires the uloop to run).
		// It also makes construction safe when ubusd is down at start-up.
		ubus(const std::string& socket = "", int timeout = 30, bool auto_reconnect = true);
		virtual ~ubus();

};

inline std::ostream& operator <<(std::ostream& os, const ubus::exception& e) {
	os << e.what();
	return os;
}
