#!/bin/bash

local_src=/Users/user1/udemy/CPP/UdemyCPP/registrationServer
host=192.168.56.102

#copy files
scp ${local_src}/*.h ${local_src}/*.cpp root@${host}:/root/register_server/src

#run build
ssh root@${host} register_server/build.sh

#verify success
success=`ssh root@${host} ls /root/register_server/sync_server`

if [ $success == "/root/register_server/sync_server" ]
then
  #backup old
  if [ -f "sync_server" ]
  then
    mv sync_server sync_server.`date "+%Y.%m.%d.%H.%M.%S"`
  fi
  # pull down sync_server
  scp root@${host}:/root/register_server/sync_server .
  # replace local header files
  rm -rf /opt/jsonP/include/*.h
  cp ${local_src}/*.h /opt/jsonP/include/
else
  echo "Build may have failed"
fi
