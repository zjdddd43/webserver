CXX = g++
CFLAGS = -std=c++11

TARGET = webserver
OBJS = http_conn.cpp main.cpp sql.cpp log.cpp

all: $(OBJS)
	$(CXX) -g -o $(TARGET) $(OBJS) -lpthread -lssl -lcrypto `mysql_config --cflags --libs`
