UBUSCPP_DIR?=ubuscpp
INCLUDES += -I./$(UBUSCPP_DIR)/include
UBUS_LIBS:=-lubox -lblobmsg_json -lubus -luci

UBUS_SRV_OBJS:= \
	objs/ubus.o
UBUS_CLI_OBJS:= \
	objs/ubus-cli.o

objs/ubus.o: $(UBUSCPP_DIR)/src/ubus.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/ubus-cli.o: $(UBUSCPP_DIR)/src/ubus-cli.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;
