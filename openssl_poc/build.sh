#!/bin/sh

gcc -o test test.c -isystem . -L . -lssl -lcrypto -ldl

