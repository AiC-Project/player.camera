
CC?=gcc

all:
	$(CC) camera-service.c misc.c camera-format-converters.c camera-capture-ffmpeg.c config_env.c net_pack.c logger.c remote_command.c -lavcodec -lavformat -lavutil -lswscale -ggdb -Wall -O3 -o camera-service -lrabbitmq -lpthread `pkg-config --cflags glib-2.0` -lglib-2.0

debug:
	$(CC) camera-service.c misc.c camera-format-converters.c camera-capture-ffmpeg.c config_env.c net_pack.c logger.c remote_command.c -lavcodec -lavformat -lavutil -lswscale -ggdb -Wall -Wextra -fsanitize=address -fstack-protector -DFORTIFY_SOURCE=2 -Og -o camera-service -lrabbitmq -lpthread `pkg-config --cflags glib-2.0` -lglib-2.0

clean:
	rm -f camera-service

#
# Build everything within a docker container, by sharing the source volume, then build the runtime image with the resulting binaries.
# Build dependencies are not required on the host, except for the make command and Docker.
#

docker-all: clean docker-wrapper docker-make docker-images

wrap = docker run --rm -ti -v ${CURDIR}/:/home/volume -ti aic.camera-wrap

docker-wrapper:
	docker build --build-arg USER_ID=$(shell id -u) --build-arg GROUP_ID=$(shell id -g) -t aic.camera-wrap -f wrap.Dockerfile .

docker-make:
	$(wrap) make

docker-images:
	docker build -t aic.camera -f camera.Dockerfile .

