
CC= gcc -no-pie

CFLAGS= -g -march=native

# Comment this line to disable address check on login,
# if you use the auto exchange feature...
CFLAGS += -DNO_EXCHANGE

LDLIBS=iniparser/libiniparser.a -lpthread -lgmp -lm -lstdc++ -lssl -lcrypto 


SOURCES=proxy.cpp util.cpp list.cpp job.cpp share.cpp socket.cpp client.cpp client_core.cpp\
	object.cpp json.cpp base58.cpp

CFLAGS += -DHAVE_CURL
LDCURL = $(shell /usr/bin/pkg-config --static --libs libcurl)
LDFLAGS += $(LDCURL)

OBJECTS=$(SOURCES:.cpp=.o)
OUTPUT=stratum-proxy

CODEDIR1=iniparser

.PHONY: projectcode1
all: projectcode1 $(SOURCES) $(OUTPUT)

projectcode1:
	$(MAKE) -C $(CODEDIR1)

$(SOURCES): proxy.h util.h

$(OUTPUT): $(OBJECTS)
	$(CC) $(OBJECTS) $(LDLIBS) $(LDFLAGS) -o $@

.cpp.o:
	$(CC) $(CFLAGS) -c $<

.c.o:
	$(CC) $(CFLAGS) -c $<

clean:
	rm -f *.o


