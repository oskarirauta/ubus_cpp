UBUSCPP_DIR?=ubus
INCLUDES += -I./$(UBUSCPP_DIR)/include
UBUS_LIBS:=-lubox -lblobmsg_json -lubus -luci

UBUS_OBJS:= \
	objs/ubus-server.o \
	objs/ubus-client.o

objs/ubus-server.o: $(UBUSCPP_DIR)/src/ubus-srv.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/ubus-client.o: $(UBUSCPP_DIR)/src/ubus-cli.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;
