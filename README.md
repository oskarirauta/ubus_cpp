[![License:MIT](https://img.shields.io/badge/License-MIT-blue?style=plastic)](LICENSE)

### ubus_cpp
Openwrt ubus c++ wrapper library

Very minimal library for c++ to handle ubus service or make calls to services.
More functions are available if you use ubus C library without wrapper, but
using this library, makes interaction with ubus very simple.

### usage
Check provided service and client example program.
To execute these programs, you will either need to be root, or execute with
sudo. For client program to work, server needs to be running on background,
or in another session.

### json
[json_cpp](https://github.com/oskarirauta/json_cpp) is used for json operations.
Previous versions used strings to allow _any_ json library user wants.
After thought, I decided to go this way- but you can always use [release v1.0.0](https://github.com/oskarirauta/ubus_cpp/releases/tag/1.0.0)
which uses strings if you want to avoid usage of json_cpp - after all, it
requires pretty new C++ standard as a caveat - or might not be to everyone's
liking.

### notes
version 3 fixes problems with version 2 with quite less changes to
code using this library. New version also allows simultaneous client
connections from uloop "tasks".
