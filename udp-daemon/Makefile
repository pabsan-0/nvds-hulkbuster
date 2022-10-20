CUDA_VER?=11.6
ifeq ($(CUDA_VER),)
  $(error "CUDA_VER is not set")
endif

APP:= hulkbuster

TARGET_DEVICE = $(shell gcc -dumpmachine | cut -f1 -d -)

NVDS_VERSION:=6.1

LIB_INSTALL_DIR?=/opt/nvidia/deepstream/deepstream-$(NVDS_VERSION)/lib/

SRCS:= main.c gstcustommeta.c gstfdcontrol.c

INCS:= $(wildcard *.h)

PKGS:= gstreamer-1.0 gstreamer-rtp-1.0 gstreamer-controller-1.0

OBJS:= $(SRCS:.c=.o)

CFLAGS+= -I/opt/nvidia/deepstream/deepstream/sources/includes \
		-I /usr/local/cuda-$(CUDA_VER)/include

CFLAGS+= $(shell pkg-config --cflags $(PKGS))

LIBS:= $(shell pkg-config --libs $(PKGS))

LIBS+= -L/usr/local/cuda-$(CUDA_VER)/lib64/ -lcudart -lnvdsgst_helper -lm \
		-L$(LIB_INSTALL_DIR) -lnvdsgst_meta -lnvds_meta -lnvds_yml_parser \
		-lcuda -Wl,-rpath,$(LIB_INSTALL_DIR)

all: $(APP) receiver

%.o: %.c $(INCS) Makefile
	$(CC) -c -Wall -g3 -o $@ $(CFLAGS) $<

$(APP): $(OBJS) Makefile
	$(CC) -o $(APP) $(OBJS) $(LIBS)
	rm main.o


clean:
	rm -rf $(OBJS) $(APP) receiver.o



## This rule was added on top of NIVIDIA's file, plus referenced in rule 'all'
receiver:
	gcc -Wall -g3  receiver.c -o $@.o `pkg-config --cflags --libs gstreamer-1.0 gstreamer-rtp-1.0`

test: all
	# SEND STDIN INPUT TO START
	GST_DEBUG=1 ./hulkbuster
