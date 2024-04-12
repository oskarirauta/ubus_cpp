#include <atomic>
#include <memory>
#include <map>

extern "C" {
#include <libubox/uloop.h>
}

#include "uloop.hpp"

static std::atomic<bool> uloop_running = false;
static std::atomic<bool> uloop_should_exit = false;

unsigned long next_id = 0;
static std::map<size_t, std::function<int()>> callbacks;
static std::map<size_t, std::unique_ptr<uloop_timeout>> handlers;

static uloop_timeout exit_task;

void common_task(uloop_timeout* t) {

	for ( auto it = handlers.begin(); it != handlers.end(); it++ ) {

		if ( it -> second.get() == t ) {

			size_t key = it -> first;
			int renew = 0;
			if ( auto it2 = callbacks.find(key); it2 != callbacks.end()) {
				int renew2 = it2 -> second();
				renew = renew2;
			}

			if ( renew > 0 ) {

				uloop_timeout_set(t, renew);
				uloop_timeout_add(t);

			} else {

				if ( callbacks.find(key) != callbacks.end())
					callbacks.erase(key);

				auto& deleter = handlers[key].get_deleter();
				auto* released = handlers[key].release();
				deleter(released);
				handlers.erase(key);
			}

			return;
		}
	}
}

void exit_uloop_task(uloop_timeout* t) {
	uloop_end();
}

size_t uloop::task::size() {

	return callbacks.size();
}

void uloop::task::add(std::function<int()> callback, int delay) {

	handlers.emplace(next_id, std::unique_ptr<uloop_timeout>(new uloop_timeout()));
	callbacks.emplace(next_id, callback);
	handlers[next_id] -> cb = common_task;

	uloop_timeout_set(handlers[next_id].get(), delay);
	uloop_timeout_add(handlers[next_id].get());
	next_id++;
}

bool uloop::is_running() {
	return uloop_running.load(std::memory_order_relaxed);
}

void uloop::run() {

	if ( uloop_running.load(std::memory_order_relaxed))
		return;

	uloop_running.store(true, std::memory_order_relaxed);

	uloop_run();
	uloop_done();

	for ( auto it = handlers.begin(); it != handlers.end(); it++ ) {

		auto& deleter = handlers[it -> first].get_deleter();
		auto* released = handlers[it -> first].release();
		deleter(released);
	}

	handlers.clear();
	callbacks.clear();
	uloop_running.store(false, std::memory_order_relaxed);

}

void uloop::exit(int timeout) {

	if ( !uloop_running.load(std::memory_order_relaxed))
		return;

	exit_task.cb = exit_uloop_task;
	uloop_timeout_set(&exit_task, timeout);
	uloop_timeout_add(&exit_task);
}
