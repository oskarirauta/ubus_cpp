UBUSCPP_DIR?=./ubus
INCLUDES += -I$(UBUSCPP_DIR)/include
LDFLAGS += -lubox -lblobmsg_json -lubus -luci

UBUS_OBJS:= \
	objs/ubus.o \
	objs/uloop.o

objs/ubus.o: $(UBUSCPP_DIR)/src/ubus.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

objs/uloop.o: $(UBUSCPP_DIR)/src/uloop.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;
