#!/bin/sh

if [ $# -eq 0 ]; then
  echo "Provide 'dial', 'listen', 'broadcast'"
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

else
  echo "Allowed args are: dial, listen, broadcast"
fi
