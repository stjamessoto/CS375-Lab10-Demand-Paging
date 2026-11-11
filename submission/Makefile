CXX      ?= g++
CXXFLAGS ?= -std=c++17 -Wall -O2

vmsim: src/*.cpp src/*.h
	$(CXX) $(CXXFLAGS) -o vmsim src/*.cpp

run: vmsim
	./vmsim

clean:
	rm -rf vmsim output
