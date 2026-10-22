CXX = g++
CXXFLAGS = -Wall -Wextra -std=c++17

all: tsamclient tsamserver

tsamclient: tsamclient.cpp
	$(CXX) $(CXXFLAGS) tsamclient.cpp -o tsamclient

tsamserver: tsamserver.cpp
	$(CXX) $(CXXFLAGS) tsamserver.cpp -o tsamserver

clean:
	rm -f tsamclient tsamserver

.PHONY: all clean%            