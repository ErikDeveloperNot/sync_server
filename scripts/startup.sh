#!/bin/bash

export LD_LIBRARY_PATH=/opt/openssl-1.1.1c/lib
home=/opt/sync_server

cd $home
one=1


for x in 9 8 7 6 5 4 3 2 1
do
  if [ -f server.log.${x} ]
  then
    ext=$((x + one))
    mv server.log.${x} server.log.${ext}
  fi
done

if [ -f server.log ]
then
  mv server.log server.log.1
fi

echo "" >> server.log
echo "" >> server.log
echo "Starting Sync Server" >> server.log
echo `date` >> server.log

nohup ./sync_server server.config > server.log 2>&1 &
