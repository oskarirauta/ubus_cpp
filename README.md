# ubuscpp
Openwrt ubus c++ wrapper library

Very minimal library for c++ to handle ubus service or make calls to services.
More functions are available if you use ubus C library without wrapper.

### usage
Check service and client examples. To use client, you need to run service
in the background/other terminal. Remember that you must run both of
these as root, sudo is sufficient.

### json
json is always imported/exported as a std::string, so you can use what
ever json implementation that supports std::string as source/dump target.

### notes
I am not very proud on how I made client (call) but it works.

### License
MIT
