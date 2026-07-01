#include <atomic>
#include <memory>
#include <map>

extern "C" {
#include <libubox/uloop.h>
}

#include "uloop.hpp"

static std::atomic<bool> uloop_running = false;
static std::atomic<bool> uloop_initialized = false;

static std::atomic<unsigned long> next_id = 0;
static std::map<size_t, std::function<int()>> callbacks;
static std::map<size_t, std::unique_ptr<uloop_timeout>> handlers;

static uloop_timeout exit_task = {};

void common_task(uloop_timeout* t) {

    for ( auto it = handlers.begin(); it != handlers.end(); ) {

        if ( it -> second.get() == t ) {

            size_t key = it -> first;
            int renew = 0;
            if ( auto it2 = callbacks.find(key); it2 != callbacks.end()) {
                renew = it2 -> second();
            }

            if ( renew > 0 ) {

                uloop_timeout_set(t, renew);
                uloop_timeout_add(t);
                ++it;

            } else {

                callbacks.erase(key);
                it = handlers.erase(it);
            }

            return;

        } else ++it;
    }
}

void exit_uloop_task(uloop_timeout* t) {
	uloop_end();
}

size_t uloop::task::size() {

	return callbacks.size();
}

void uloop::task::add(std::function<int()> callback, int delay) {

	auto [it, inserted] = handlers.emplace(next_id, std::make_unique<uloop_timeout>());
	if ( !inserted )
		return; // or handle error

	callbacks.emplace(next_id.load(), std::move(callback));
	it -> second -> cb = common_task;

	uloop_timeout_set(it -> second.get(), delay);
	uloop_timeout_add(it -> second.get());
	next_id++;
}

bool uloop::is_running() {
	return uloop_running.load(std::memory_order_relaxed);
}

bool uloop::is_initialized() {
	return uloop_initialized.load(std::memory_order_relaxed);
}

void uloop::init() {

	if (uloop_initialized.load(std::memory_order_relaxed))
		return;

	uloop_init();
	uloop_initialized.store(true, std::memory_order_relaxed);
}

void uloop::run() {

	if ( uloop_running.load(std::memory_order_relaxed))
		return;

	uloop::init();
	uloop_running.store(true, std::memory_order_relaxed);

	uloop_run();
	uloop_done();

	handlers.clear();
	callbacks.clear();

	uloop_running.store(false, std::memory_order_relaxed);
	uloop_initialized.store(false, std::memory_order_relaxed);
}

void uloop::exit(int timeout) {

	if ( !uloop_running.load(std::memory_order_relaxed))
		return;

	memset(&exit_task, 0, sizeof(exit_task));
	exit_task.cb = exit_uloop_task;
	uloop_timeout_set(&exit_task, timeout);
	uloop_timeout_add(&exit_task);
}
