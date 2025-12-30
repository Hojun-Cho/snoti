#!/bin/sh

pkill snoti
./snoti -n 6 -t 10000 -s coin.wav "$@"
