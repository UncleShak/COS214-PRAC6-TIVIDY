CXX ?= g++
CXXFLAGS ?= -std=c++11 -Wall -Wextra -pedantic -O2 -I. -Iobserver -Ihistory -Iproxy -Itests

SRCS = observer/Event.cpp \
       observer/EventSource.cpp \
       observer/NotificationObserver.cpp \
       observer/MonitoringObserver.cpp \
       history/HistoryEntry.cpp \
       history/InstanceHistory.cpp \
       proxy/RoleAccessPolicy.cpp \
       proxy/AccessControlProxy.cpp \
       tests/test_sloth.cpp

OBJS = $(SRCS:.cpp=.o)
TARGET = test_sloth

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all test clean
