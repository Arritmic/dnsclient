HEADERS = AAAAResourceRecord.h AResourceRecord.h DomainName.h Message.h MXResourceRecord.h NSResourceRecord.h
OBJECTS = AAAAResourceRecord.o AResourceRecord.o DomainName.o main.o Message.o MXResourceRecord.o NSResourceRecord.o ResourceRecord.o
CPPFLAGS = -Wall -g -std=c++14

.PHONY: default all clean

default: dnsclient
all: default

%.o: %.cc $(HEADERS)
	g++ $(CPPFLAGS) -c $< -o $@

dnsclient: $(OBJECTS)
	g++ $(CPPFLAGS) $(OBJECTS) -o $@

clean:
	-rm -f $(OBJECTS)
	-rm -f dnsclient
