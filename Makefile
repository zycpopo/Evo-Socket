.PHONY: all echo http check clean
all: echo http
echo:
	$(MAKE) -C source/echo
http:
	$(MAKE) -C source/http
check:
	$(MAKE) -C test check
clean:
	$(MAKE) -C source/echo clean
	$(MAKE) -C source/http clean
	$(MAKE) -C test clean
