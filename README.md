[![License:MIT](https://img.shields.io/badge/License-MIT-blue?style=plastic)](LICENSE)

# ubus_cpp

A small, modern C++ wrapper around OpenWrt's **ubus** (the libubus/libubox C API).
It lets you expose ubus objects, call other services, broadcast and listen to
events, subscribe to object notifications, reply to requests asynchronously,
and batch multiple calls together — without touching the C API directly.

JSON is handled with [json_cpp](https://github.com/oskarirauta/json_cpp); every
argument and result is a `JSON` value.

## Features

- Register ubus **objects** with synchronous **or deferred** method handlers.
- **Synchronous** and **asynchronous** calls to other services.
- **Batch calls**: execute multiple synchronous calls in one go.
- **Events**: broadcast (`send_event`) and listen (`add_event_handler`).
- **Notifications**: subscribe to an object's notifications (`subscribe`) and
  send notifications to your own subscribers (`notify`).
- **uloop** task scheduling (one-shot or self-renewing timers).
- **Auto-reconnect**: survives a `ubusd` restart and re-registers everything.

## Dependencies

- libubox + libubus + blobmsg_json (OpenWrt). *(uci is **not** required.)*
- [json_cpp](https://github.com/oskarirauta/json_cpp) - included as a submodule.
- A C++17 compiler.

```sh
git clone --recursive https://github.com/oskarirauta/ubus_cpp.git
cd ubus_cpp
make            # builds the example "server" (run as "client" for the client demo)
```

On Alpine the packages are `libubox libubox-dev libblobmsg ubus ubus-dev`
(from edge/testing); on OpenWrt the corresponding `-dev` packages.

## Quick start

```cpp
#include "ubus.hpp"

ubus* srv;

int hello(const std::string& method, const JSON& req, JSON& res) {
    res["hello"] = "world";
    return 0;
}

int main() {
    srv = new ubus;                                  // connect (auto-reconnect on)
    srv->add_object("my_service", {
        { .name = "hello", .cb = hello },
    });
    uloop::run();                                    // serve until uloop::exit()
    delete srv;
}
```

Call it from the shell with `ubus call my_service hello`.

## API

### Construction

```cpp
ubus(const std::string& socket = "", int timeout = 30, bool auto_reconnect = true);
```

- `socket` - ubus socket path; empty = default.
- `timeout` - seconds for synchronous `call()`.
- `auto_reconnect` - reconnect after a `ubusd` restart and automatically
  re-register all objects and event handlers. Also makes construction safe when
  `ubusd` is down at start-up. Requires `uloop::run()` to be running for the
  reconnect to happen. Pass `false` for the classic connect-once behaviour.

Only **one** `ubus` instance may exist at a time.

### Objects and methods

```cpp
void add_object(const std::string& name, const std::vector<method>& methods);

struct method {
    std::string name;
    std::function<void(const std::string&, const JSON&, JSON&)>          cb;   // sync
    std::function<void(const std::string&, const JSON&, ubus::request)>  dcb;  // deferred
    std::vector<std::pair<std::string, JSON::TYPE>>                      hints;
};
```

- **`cb` (synchronous)**: fill the `JSON&` result; it is sent when you return.
- **`dcb` (deferred)**: if set, it takes precedence over `cb`. You get a
  `ubus::request` handle and reply later (see below). Use this for slow work so
  the event loop is not blocked.
- **`hints`**: optional argument name/type policy.

```cpp
int hello(const std::string& m, const JSON& req, JSON& res) {
    res["echo"] = req;                 // req holds the call arguments
    return 0;
}

void slow(const std::string& m, const JSON& req, ubus::request r) {
    // reply now, or keep r and reply later from a task / async callback:
    uloop::task::add([r]() -> int {
        JSON res; res["done"] = true;
        r.reply(res);                  // sends the response and completes the request
        return 0;                      // one-shot task
    }, 500);
}

srv->add_object("svc", {
    { .name = "hello", .cb = hello },
    { .name = "slow",  .dcb = slow },
});
```

### The deferred request handle

```cpp
class request {
    void reply(const JSON& res = JSON(), int status = 0) const;  // reply + complete
    bool valid() const;                                          // not replied yet
};
```

`request` is copyable (shared state) - store it in your own structures and reply
whenever the work is done. Replying more than once is a no-op.

### Calls

```cpp
// synchronous: blocks until reply/timeout; throws ubus::exception on failure
JSON call(const std::string& obj, const std::string& cmd, const JSON& args = JSON()) const;

// batch: execute multiple calls sequentially. Returns results in order.
// If any call fails, throws ubus::exception.
std::vector<JSON> call_batch(const std::vector<std::tuple<std::string, std::string, JSON>>& calls) const;

// asynchronous: returns immediately; cb fires from uloop (ok=false on error)
void call_async(const std::string& obj, const std::string& cmd, const JSON& args,
                std::function<void(const JSON& result, bool ok)> cb);
```

```cpp
JSON board = srv->call("system", "board");          // sync

auto results = srv->call_batch({
    { "system", "board", JSON() },
    { "system", "info",  JSON() },
    { "network.interface.lan", "status", JSON() },
});

srv->call_async("system", "board", JSON(), [](const JSON& res, bool ok) {
    if (ok) std::cout << res["model"].to_string() << "\n";
});
```

### Events

```cpp
void send_event(const std::string& id, const JSON& data = JSON()) const;
void add_event_handler(const std::string& pattern,
                       std::function<void(const std::string& id, const JSON& data)> cb);
```

`pattern` supports a trailing `*` wildcard. (ubusd does not deliver an event back
to the process that sent it.)

```cpp
srv->add_event_handler("network.*", [](const std::string& id, const JSON& data) {
    std::cout << "event " << id << ": " << data << "\n";
});
srv->send_event("my.event", JSON());
```

### Notifications (subscriber / notifier)

Objects can push notifications to their subscribers. Other processes can
subscribe to these notifications without making repeated polling calls.
Notifier — inside an object that has subscribers:

```cpp

// Send a notification to all current subscribers of 'obj_name'.
// 'type' is an arbitrary string identifying the notification.
// timeout < 0 means wait forever, 0 non-blocking, >0 ms timeout.
void notify(const std::string& obj_name, const std::string& type,
            const JSON& data = JSON(), int timeout = -1) const;
```

```cpp

// In a method handler or a uloop task:
srv->notify("svc", "update", JSON({ {"progress", 42} }));
Subscriber — listen to another object's notifications:
```

```cpp

// Subscribe to an object's notifications.
// cb(type, data) fires for every notification received.
// The returned handle unsubscribes automatically when destroyed.
subscription subscribe(const std::string& obj_name,
                       std::function<void(const std::string& type, const JSON& data)> cb);
```

```cpp
auto sub = srv->subscribe("svc", [](const std::string& type, const JSON& data) {
    std::cout << "notification " << type << ": " << data << "\n";
});

// ... later, or let it go out of scope to unsubscribe automatically:
// sub = ubus::subscription();   // explicit unsubscribe
```

### uloop

```cpp
uloop::task::add(std::function<int()> task, int delay_ms = 0);  // return >0 ms to renew, else one-shot
void uloop::run();                                              // run the loop
void uloop::exit(int timeout = 50);                             // stop the loop
bool uloop::is_running();
bool uloop::is_initialized();
```

### Errors

`call()`, `add_object()` and construction throw `ubus::exception` (`.what()`,
`.code()`). Asynchronous paths report failure through their callback's `ok` flag
instead of throwing.

### Examples



See the examples/ directory for complete, buildable programs:
Table

| File             | Description                                                  |
| ---------------- | ------------------------------------------------------------ |
| `main.cpp`       | Registers an object with sync and deferred methods.          |
|                  | Calls the server object synchronously and asynchronously.    |
| `subscriber.cpp` | Subscribes to another object's notifications.                |
| `notifier.cpp`   | Registers an object and pushes notifications to subscribers. |

## Notes

- Earlier (v1.x) the API used JSON strings so you could plug in any JSON library;
  from v2 it uses json_cpp. Use [release 1.0.0](https://github.com/oskarirauta/ubus_cpp/releases/tag/1.0.0)
  if you want the string-based API.
- call_batch is implemented as sequential synchronous calls; it blocks until
  all calls are done (or one throws). For fully parallel execution, use multiple
  call_async calls instead.
- Only one ubus instance may exist at a time because the C API uses a global
  singleton internally.
