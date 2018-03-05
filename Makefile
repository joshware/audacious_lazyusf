all: usf.so

CFLAGS := -I. -Ilazyusf2 -g

usf.so: plugin.o psftag.o lazyusf2/liblazyusf.a
	g++ -o usf.so -shared -fPIC plugin.o psftag.o lazyusf2/liblazyusf.a  -laudcore -laudtag

plugin.o: plugin.cpp plugin.h psftag.h
	g++ -o plugin.o -c -fPIC -std=c++11 $(CFLAGS) plugin.cpp

psftag.o: psftag.c psftag.h
	g++ -o psftag.o -c -fPIC -std=c++11 $(CFLAGS) psftag.c

install: usf.so
	cp usf.so /usr/lib/x86_64-linux-gnu/audacious/Input/usf.so

clean:
	rm -f usf.so plugin.o

# include ./lazyusf2/Makefile
