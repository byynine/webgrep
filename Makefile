bld/main: src/main.c | bld
	gcc -o $@ -lcurl -I/usr/include/libxml2 -lxml2 $<

bld:
	mkdir -p bld

run: bld/main
	bld/main
clean:
	rm -rf bld/
