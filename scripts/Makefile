INCLUDES=-I /opt/jsonP/include -I /opt/openssl-1.1.1c/include -I /opt/hiredis
CXX=/opt/rh/devtoolset-8/root/bin/g++

# Max performance, 03 runs into issues
CXXFLAGS=-g -O2 -fgcse-after-reload -fipa-cp-clone -floop-interchange -floop-unroll-and-jam -fpeel-loops -fpredictive-commoning -fsplit-paths -ftree-loop-distribute-patterns -ftree-loop-distribution -ftree-loop-vectorize -ftree-partial-pre -pthread

# Debug build
#CXXFLAGS=-g -O0 -pthread

# Default build
#CXXFLAGS=-pthread

LIBS=-lpq -L/opt/openssl-1.1.1c/lib -L/opt/jsonP -ljsonP -lssl -lcrypto -L/opt/hiredis/usr/local/lib -lhiredis
LDFLAGS=-Wl,-rpath,/opt/jsonP,-rpath,/opt/openssl-1.1.1c/lib,-rpath,/opt/hiredis/usr/local/lib
FINAL_DIR=/root/register_server

OBJ=Config.o register_server_exception.o config_http.o server.o data_store_connection.o main.o sync_handler.o RedisStore.o

#OBJ = ./src/file_chunk_impl.cpp ./src/jsonP_json.cpp ./src/jsonP_push_parser.cpp ./src/jsonP_buffer_parser.cpp ./src/jsonP_parser.cpp



sync_server: ${OBJ}
	${CXX} ${CXXFLAGS} ${INCLUDES} -o ${FINAL_DIR}/$@ $^ ${LIBS} ${LDFLAGS}

${OBJ}:
	${CXX} ${CXXFLAGS} ${INCLUDES} -c ./src/*.cpp

clean:
	-rm -f *.o core *.core
	exit

all: sync_server clean
