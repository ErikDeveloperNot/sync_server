#!/bin/bash

kill `ps -ef | grep "\.\/sync_server server.config" | awk '{print $2}'`
