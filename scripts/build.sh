#!/bin/bash

home=/root/register_server
src=${home}/src

cd ${home}

if [ -f sync_server ]
then
  echo "deleting sync_server"
  rm -f sync_server
fi

#export LD_LIBRARY_PATH=/opt/openssl-1.1.1c/lib:${LD_LIBRARY_PATH=}

# default build
#/opt/rh/devtoolset-8/root/bin/g++ -I /opt/jsonP/include -I /opt/openssl-1.1.1c/include -pthread -o ${home}/sync_server ${src}/*.cpp -lpq -L/opt/openssl-1.1.1c/lib -L/opt/jsonP -ljsonP -lssl -lcrypto -Wl,-rpath,/opt/jsonP,-rpath,/opt/openssl-1.1.1c/lib

# max performance build
#/opt/rh/devtoolset-8/root/bin/g++ -g -O2 -fgcse-after-reload -fipa-cp-clone -floop-interchange -floop-unroll-and-jam -fpeel-loops -fpredictive-commoning -fsplit-paths -ftree-loop-distribute-patterns -ftree-loop-distribution -ftree-loop-vectorize -ftree-partial-pre -I /opt/jsonP/include -I /opt/openssl-1.1.1c/include -pthread -o ${home}/sync_server ${src}/*.cpp -lpq -L/opt/openssl-1.1.1c/lib -L/opt/jsonP -ljsonP -lssl -lcrypto -Wl,-rpath,/opt/jsonP,-rpath,/opt/openssl-1.1.1c/lib

#/opt/rh/devtoolset-8/root/bin/g++ -O2 -I /opt/jsonP/include -I /opt/openssl-1.1.1c/include -pthread -o ${home}/sync_server ${src}/*.cpp -lpq -L/opt/openssl-1.1.1c/lib -L/opt/jsonP -ljsonP -lssl -lcrypto -Wl,-rpath,/opt/jsonP,-rpath,/opt/openssl-1.1.1c/lib

# debugger build
#/opt/rh/devtoolset-8/root/bin/g++ -g -I /opt/jsonP/include -I /opt/openssl-1.1.1c/include -pthread -o ${home}/sync_server ${src}/*.cpp -lpq -L/opt/openssl-1.1.1c/lib -L/opt/jsonP -ljsonP -lssl -lcrypto -Wl,-rpath,/opt/jsonP,-rpath,/opt/openssl-1.1.1c/lib

make all

#rm -f ${src}/*.cpp ${src}/*.h

cp -f ${home}/sync_server /opt/sync_server/
