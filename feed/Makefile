CXX = g++
CXXFLAGS += -g
CXXFLAGS += -Og
CXXFLAGS += -Wall
CXXFLAGS += -Werror
CXXFLAGS += -Wextra
CXXFLAGS += -pthread
CXXFLAGS += -lzmq
CXXFLAGS += -lixwebsocket
CXXFLAGS += -lssl
CXXFLAGS += -lcrypto
CXXFLAGS += -lz
CXXFLAGS += -std=c++20
CXXFLAGS += -MMD -MP


HOME=/home/aiden/projects/aidtrade
INC += -I$(HOME)/include
LIB += -L/usr/lib/x86_64-linux-gnu
# source file name without extension
TARGET = $(shell basename $(FILE) .cpp)

all: $(TARGET)

$(TARGET): $(FILE)
	@$(CXX)  $< -o $@ $(CXXFLAGS) $(INC) $(LIB) 

%.o:%.cpp
	@$(CXX) $@ -c $<

.PHONY: all clean run

clean:
	@ls -p | grep -v / | grep -v '\.' | grep -v 'Makefile' | grep -v '.mk' | xargs rm -f
	@rm -rf *.o *.d

run: all
	./$(TARGET)

-include $(wildcard *.d)
