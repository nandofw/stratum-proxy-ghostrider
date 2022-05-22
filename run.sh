#!/bin/bash

ulimit -n 10240
ulimit -u 10240


while [ -e ${1}.conf ]; do
        ./stratum-proxy $1
	sleep 1
done
exec bash

