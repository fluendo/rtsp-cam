CFLAGS += $(shell pkg-config --cflags gstreamer-1.0 gstreamer-app-1.0)
LDFLAGS += $(shell pkg-config --libs gstreamer-1.0 gstreamer-app-1.0)

CFLAGS += -Wall -Wextra -pedantic

TESTS := \
	 test_without_nvvidconv \
	 test_with_nvvidconv \
	 test_with_nvvidconv_big_output_buffers \
	 test_with_nvvidconv_sync_start \

.PHONY: all test test_all $(TESTS) outdir clean

all: test_all
test_all:
	for t in $(TESTS); do \
		$(MAKE) -B $$t; \
	done

test_with_nvvidconv_sync_start: CFLAGS += -D USE_NVVIDCONV
test_with_nvvidconv_sync_start: CFLAGS += -D CONSUMMER_START_AT_BUFFER=1
test_with_nvvidconv_sync_start: OUTDIR = with_nvvidconv_sync_start
test_with_nvvidconv_sync_start: test

test_with_nvvidconv_big_output_buffers: CFLAGS += -D USE_NVVIDCONV
test_with_nvvidconv_big_output_buffers: CFLAGS += -D NVVIDCONV_OUTPUT_BUFFERS=50
test_with_nvvidconv_big_output_buffers: OUTDIR = with_nvvidconv_big_out_buffers
test_with_nvvidconv_big_output_buffers: test

test_with_nvvidconv: CFLAGS += -D USE_NVVIDCONV
test_with_nvvidconv: OUTDIR = with_nvvidconv
test_with_nvvidconv: test

test_without_nvvidconv: OUTDIR = without_nvvidconv
test_without_nvvidconv: test

test: nvmm_test | outdir
	rm -f "out/$(OUTDIR)/*"
	./$<

outdir:
	mkdir -pv "out/$(OUTDIR)"

nvmm_test: main.c
	$(CC) $(CFLAGS) -D OUTDIR="\"out/$(OUTDIR)\"" "$<" $(LDFLAGS) -o "$@"

clean:
	rm -fv nvmm_test
	rm -fr out/
