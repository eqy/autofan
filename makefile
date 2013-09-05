CFLAGS = -Wall -Wextra
LDFLAGS = -L/usr/lib -L/usr/local/lib -I/usr/include/libxml2 
LIBS = -lcurl -ltidy -lxml2 -lboost_regex
autofan : src/main.cpp src/tltopic.cpp src/autofan.cpp
	g++ -o autofan.o $(CFLAGS) src/main.cpp src/tltopic.cpp src/autofan.cpp $(LDFLAGS) $(LIBS)
