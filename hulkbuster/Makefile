CUDA_VER?=11.6
ifeq ($(CUDA_VER),)
  $(error "CUDA_VER is not set")
endif

APP:= bin/hulkbuster

TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)

NVDS_VERSION:=6.1

LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/

SRCS:= $(wildcard src/*.c)

INCS:= $(wildcard include/*.h)

PKGS:= gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-controller-1.0

OBJS:= $(SRCS:src/*.c=bin/*.o)

CFLAGS+= -I/opt/nvidia/deepstream/deepstream/sources/includes \
		 -I/usr/local/cuda-$(CUDA_VER)/include 

CFLAGS+= $(shell pkg-config --cflags $(PKGS))

LIBS:= $(shell pkg-config --libs $(PKGS))

LIBS+= -L/usr/local/cuda-$(CUDA_VER)/lib64/ -lcudart -lnvdsgst_helper -lm \
		-L$(LIB_INSTALL_DIR) -lnvdsgst_meta -lnvds_meta -lnvds_yml_parser \
		-lcuda -Wl,-rpath,$(LIB_INSTALL_DIR)

hulkbuster: $(SRCS) $(INCS)
	$(CC) -Wall -g3 -o $(APP) $(OBJS) $(LIBS) $(CFLAGS) -I./include

clean:
	rm bin/*

test: all
	GST_DEBUG=1 bin/hulkbuster
