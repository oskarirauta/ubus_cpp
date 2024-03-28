#pragma once

#include "ubus.hpp"

int ubus_get(const std::string& method, const std::string& msg, std::string& result);
int ubus_list(const std::string& method, const std::string& msg, std::string& result);
int ubus_test(const std::string& method, const std::string& msg, std::string& result);
int ubus_test2(const std::string& method, const std::string& msg, std::string& result);
