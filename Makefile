ifndef LUA
LUA = luajit
endif
ifndef MODNAME
MODNAME = ljd
endif
ifndef TARGET
TARGET = ljd.so
endif

all: $(TARGET)

$(TARGET): ljd.c
	$(CC) -DMODNAME="$(MODNAME)" $(CFLAGS) -Iluajit/src -fPIC -shared -o $@ $< $(LDFLAGS)

compile_commands.json: Makefile
	compiledb -n make

luajit: luajit/src/luajit
	cd luajit && make

test: $(TARGET) luajit/src/luajit
	./luajit/src/luajit test.lua

clean:
	rm -f $(TARGET)

.PHONY: all luajit test clean
