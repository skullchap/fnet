#!/bin/sh

if [ $# -eq 0 ]; then
  echo "Provide 'dial', 'listen', 'broadcast', 'get'"
  exit 1
fi

if [ "$1" = "dial" ]; then
  echo "compiling dial"
  cc \
  -I ../. \
  ../fnet.c \
  dial.c -o dial

elif [ "$1" = "listen" ]; then
  echo "compiling listen"
  cc \
  -I ../. \
  ../fnet.c \
  listen.c -o listen

elif [ "$1" = "broadcast" ]; then
  echo "compiling broadcast"
  cc \
  -I ../. \
  ../fnet.c \
  broadcast.c -o broadcast

elif [ "$1" = "get" ]; then
  echo "compiling get"
  cc \
  -I ../. \
  ../fnet.c \
  get.c -o get

else
  echo "Allowed args are: dial, listen, broadcast, get"
fi
