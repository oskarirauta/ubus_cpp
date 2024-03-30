all: world

CXX?=g++
CXXFLAGS?=--std=c++17 -Wall -fPIC
LDFLAGS?=-L/lib -L/usr/lib

INCLUDES+= -I./examples/include -I./jsoncpp/include

UBUSCPP_DIR:=.
include Makefile.inc
include json/Makefile.inc

world: server client

$(shell mkdir -p objs)

objs/main.o: main.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c -o $@ $<;

server: $(JSON_OBJS) $(UBUS_OBJS) objs/main.o
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -L. $(UBUS_LIBS) $^ -o $@;

client: server
	ln -s server client

.PHONY: clean
clean:
	@rm -rf objs
	@rm -f server
	@rm -f client
