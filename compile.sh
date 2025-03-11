#!/bin/sh

if [ $# -eq 0 ]; then
  echo "Provide : 'dial' or 'listen'."
  exit 1
fi

if [ "$1" = "dial" ]; then
  echo "compiling dial"
  cc fnet.c \
  dial.c -o dial

elif [ "$1" = "listen" ]; then
  echo "compiling listen"
  cc fnet.c \
  listen.c -o listen

else
  echo "Allowed args are: dial, listen"
fi
