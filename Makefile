build:

	cd beej-plus-plus && make
	c++ --std=c++2a -static-libgcc -static-libstdc++ server.cpp $$PWD/beej-plus-plus/build/libbeej++-static.a -o server
	c++ --std=c++2a -static-libgcc -static-libstdc++ client.cpp $$PWD/beej-plus-plus/build/libbeej++-static.a -o client

install:

	cp -prv server ~/essential/bin/jserver
	cp -prv client ~/essential/bin/jclient

prepare:
	wget "https://github.com/nlohmann/json/releases/download/v3.10.5/json.hpp"

format:

	clang-format -i server.cpp
	clang-format -i client.cpp
