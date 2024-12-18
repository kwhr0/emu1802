all:
	c++ -std=c++11 -O3 -I/opt/homebrew/include -L/opt/homebrew/lib -lSDL2 main.cpp CDP1802.cpp PSG.cpp -o emu1802
