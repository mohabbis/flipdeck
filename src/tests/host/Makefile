# Host tests for the SDK-independent parts of the Flipper app (protocol + state).
# `make run` is what CI invokes.
CC      ?= cc
CFLAGS  = -std=c11 -Wall -Wextra -Wpedantic -Werror -Wconversion -Wno-sign-conversion -g -fsanitize=address,undefined -I../..
SRCDIR  = ../..

.PHONY: all test run clean

all: test_proto test_state fd_sim

test_proto: test_proto.c test_util.h $(SRCDIR)/fd_proto.c $(SRCDIR)/fd_proto.h
	$(CC) $(CFLAGS) -o $@ test_proto.c $(SRCDIR)/fd_proto.c

test_state: test_state.c test_util.h $(SRCDIR)/fd_state.c $(SRCDIR)/fd_state.h $(SRCDIR)/fd_proto.c
	$(CC) $(CFLAGS) -o $@ test_state.c $(SRCDIR)/fd_state.c $(SRCDIR)/fd_proto.c

# The Flipper's protocol/state code as a stdin/stdout process, used by the
# Swift interop test (mac/Tests/FlipDeckCoreTests/InteropTests.swift).
fd_sim: fd_sim.c $(SRCDIR)/fd_state.c $(SRCDIR)/fd_state.h $(SRCDIR)/fd_proto.c
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -o $@ fd_sim.c $(SRCDIR)/fd_state.c $(SRCDIR)/fd_proto.c

test: run

run: all
	./test_proto ../../../docs/protocol-vectors.txt
	./test_state

clean:
	rm -f test_proto test_state fd_sim
