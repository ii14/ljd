ifndef MODNAME
MODNAME = ljd
endif
ifndef TARGET
TARGET = ljd.so
endif

CFLAGS = -std=c99 -Wall -Wextra -pedantic -g

SRCS = ljd.c bp.c
OBJS = $(SRCS:.c=.o)
DEPS = $(OBJS:.o=.d)

all: $(TARGET)

%.o: %.c
	$(CC) -DMODNAME="$(MODNAME)" $(CFLAGS) -Iluajit/src -c -MMD -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -DMODNAME="$(MODNAME)" $(CFLAGS) -Iluajit/src -fPIC -shared -o $@ $(OBJS) $(LDFLAGS)

-include $(DEPS)

luajit: luajit/src/luajit
	cd luajit && make

test: $(TARGET) luajit/src/luajit
	busted --lua=./luajit/src/luajit

compile_commands.json: Makefile
	compiledb -n make

clean:
	@rm -rvf $(TARGET) $(OBJS) $(DEPS)

.PHONY: all luajit test clean
