#!/bin/bash

home=/root/register_server
src=${home}/src

if [ -f sync_server ]
then
  echo "deleting sync_server"
  rm -f sync_server
fi

export LD_LIBRARY_PATH=/opt/openssl-1.1.1c/lib:${LD_LIBRARY_PATH=}

/opt/rh/devtoolset-8/root/bin/g++ -I /opt/openssl-1.1.1c/include -pthread -o ${home}/sync_server ${src}/*.cpp -lpq -L/opt/openssl-1.1.1c/lib -lssl -lcrypto


rm -f ${src}/*.cpp ${src}/*.h

cp -f ${home}/sync_server /opt/sync_server/
