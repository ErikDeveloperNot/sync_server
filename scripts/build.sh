#!/bin/bash

home=/root/register_server
src=${home}/src

if [ -f sync_server ]
then
  echo "deleting sync_server"
  rm -f sync_server
fi

/opt/rh/devtoolset-8/root/bin/g++ -I /opt/jsonP/include -I /opt/openssl-1.1.1c/include -pthread -o ${home}/sync_server ${src}/*.cpp -lpq -L/opt/openssl-1.1.1c/lib -L/opt/jsonP -ljsonP -lssl -lcrypto -Wl,-rpath,/opt/jsonP,-rpath,/opt/openssl-1.1.1c/lib

#rm -f ${src}/*.cpp ${src}/*.h

cp -f ${home}/sync_server /opt/sync_server/
