#!/bin/bash

sudo kill -9 $(sudo netstat -tulnp | grep 12345 | awk '{print $7}' | cut -d'/' -f1)

# Find and kill all processes named './subscriber'
pids=$(pgrep -f "./subscriber")

if [ -z "$pids" ]; then
    echo "No ./subscriber processes found."
else
    echo "Killing the following ./subscriber processes: $pids"
    kill -9 $pids
    echo "All ./subscriber processes have been terminated."
fi