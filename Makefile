OPT = /opt/homebrew
#OPT = /opt/local
#OPT = /usr/local

all:
	c++ -std=c++11 -O3 -I$(OPT)/include -L$(OPT)/lib *.cpp -o emu1802 -lSDL2
