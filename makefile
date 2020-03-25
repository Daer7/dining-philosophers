all: DPP
DPP: main.cpp
	g++ main.cpp -o DPP -lncurses -pthread
