CXX = g++
CXXFLAGS = -Wall -std=c++17

TARGET = node

SRCS = main.cpp helper.cpp sender.cpp receiver.cpp socket.cpp segment.cpp
OBJS = $(SRCS:.cpp=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(OBJS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)
