# Compiler
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -pedantic
LDFLAGS  = -ldl

TARGET   = arena
SRCS     = main.cpp Arena.cpp RobotBase.cpp
OBJS     = $(SRCS:.cpp=.o)

all: $(TARGET) RobotBase.o

# Link the arena executable
$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $(OBJS) $(LDFLAGS)

# Compile RobotBase.o separately so robots can link against it
RobotBase.o: RobotBase.cpp RobotBase.h
	$(CXX) $(CXXFLAGS) -c RobotBase.cpp -o RobotBase.o

clean:
	rm -f $(OBJS) $(TARGET) RobotBase.o robots/*.so

run: all
	./$(TARGET) config.txt