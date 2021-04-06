#!/bin/bash -e
set -x
mkdir -p tmp.fuzz
echo hi > tmp.fuzz/hello.txt
clang -c -Dmain=darkhttpd -g -O2 -fsanitize=fuzzer,address ../darkhttpd.c -o fuzz_darkhttpd.o
clang++ -g -O2 -fsanitize=fuzzer,address fuzz_socket.cc fuzz_darkhttpd.o -o fuzz_socket
./fuzz_socket fuzz_socket_testcases -detect_leaks=0 -only_ascii=1
