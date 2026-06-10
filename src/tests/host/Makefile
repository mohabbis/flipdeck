CC      = gcc
CFLAGS  = -DUFBT_HOST_BUILD -I./stubs -std=c99 -Wall -Wextra -Wpedantic
SRCDIR  = ../..

.PHONY: all test run clean

all: test_profile_manager test_usb_hid test_profile_manager_target

test_profile_manager: test_profile_manager.c $(SRCDIR)/profile_manager.c
	$(CC) $(CFLAGS) -o $@ $^

test_usb_hid: test_usb_hid.c $(SRCDIR)/usb_hid.c $(SRCDIR)/usb_hid.h
	$(CC) $(CFLAGS) -o $@ test_usb_hid.c

test_profile_manager_target: test_profile_manager_target.c $(SRCDIR)/profile_manager.c
	$(CC) $(CFLAGS) -o $@ $^

test: run

run: all
	@echo "--- profile_manager ---"
	./test_profile_manager
	@echo ""
	@echo "--- usb_hid ---"
	./test_usb_hid
	@echo ""
	@echo "--- profile_manager_target ---"
	./test_profile_manager_target

clean:
	rm -f test_profile_manager test_usb_hid test_profile_manager_target
