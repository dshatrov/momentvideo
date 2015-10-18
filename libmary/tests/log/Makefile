COMMON_CFLAGS =                         \
        -ggdb                           \
        -Wno-long-long -Wall -Wextra    \
        `pkg-config --cflags libmary-1.0`

CFLAGS   = -std=gnu99 $(COMMON_CFLAGS)
CXXFLAGS = -std=c++0x $(COMMON_CFLAGS)

LDFLAGS = `pkg-config --libs libmary-1.0`
LDFLAGS += -L/opt/moment/lib -Wl,--dynamic-linker=/opt/moment/lib/ld-linux-x86-64.so.2 -Wl,-L/opt/moment/lib -Wl,-rpath=/opt/moment/lib

.PHONY: all clean

TARGETS = test__log

all: $(TARGETS)

clean:
	rm -f $(TARGETS)

