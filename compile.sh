#!/bin/sh

if [ $# -eq 0 ]; then
  echo "Provide 'dial' or 'listen'."
  exit 1
fi

if [ "$1" = "dial" ]; then
  echo "compiling dial"
  cc \
  -I ./. \
  fnet.c \
  examples/dial.c -o examples/dial

elif [ "$1" = "listen" ]; then
  echo "compiling listen"
  cc \
  -I ./. \
  fnet.c \
  examples/listen.c -o examples/listen

else
  echo "Allowed args are: dial, listen"
fi
