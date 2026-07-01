all: world

CXX?=g++
CXXFLAGS?=--std=c++17 -Wall -fPIC -g
LDFLAGS?=-L/lib -L/usr/lib

SERVER_OBJS:= objs/main.o
NOTIFIER_OBJS:= objs/notifier.o
SUBSCRIBER_OBJS:= objs/subscriber.o

UBUSCPP_DIR:=.
include Makefile.inc
include json/Makefile.inc

world: server client notifier subscriber

$(shell mkdir -p objs)

objs/main.o: examples/main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

server: $(JSON_OBJS) $(UBUS_OBJS) $(SERVER_OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@;

objs/notifier.o: examples/notifier.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

notifier: $(JSON_OBJS) $(UBUS_OBJS) $(NOTIFIER_OBJS)
	  $(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@;

objs/subscriber.o: examples/subscriber.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

subscriber: $(JSON_OBJS) $(UBUS_OBJS) $(SUBSCRIBER_OBJS)
	    $(CXX) $(CXXFLAGS) $(LDFLAGS) $^ $(LIBS) -o $@;

client: server
	@ln -s server client

.PHONY: clean
clean:
	@rm -rf objs client server notifier subscriber
