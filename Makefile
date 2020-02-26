all : usertime

usertime : main.cpp
	g++ -g -o $@ $^ --std=c++14
