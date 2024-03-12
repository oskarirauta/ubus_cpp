[![License:MIT](https://img.shields.io/badge/License-MIT-blue?style=plastic)](LICENSE)

### ubus_cpp
Openwrt ubus c++ wrapper library

Very minimal library for c++ to handle ubus service or make calls to services.
More functions are available if you use ubus C library without wrapper, but
using this library, makes interaction with ubus very simple.

### usage
Check service and client example programs.
To execute these programs, you will either need to be root, or execute with
sudo. For client program to work, srv service needs to be running on background.

### json
json is always imported/exported as a std::string, so you can use what
ever json implementation that supports std::string as source/dump target.
Example code uses [json_cpp](https://github.com/oskarirauta/json_cpp)
