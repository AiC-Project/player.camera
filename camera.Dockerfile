FROM ubuntu:16.04

RUN apt-get -y update && \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        libav-tools \
        libavcodec-ffmpeg56 \
        libavformat-ffmpeg56 \
        libswscale-ffmpeg3 \
        libglib2.0-0 \
        libpopt0 \
        librabbitmq4 && \
    apt-get clean -y && \
    rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN useradd -m developer

COPY ./camera-service /home/developer/player_camera
COPY ./start_camera /home/developer/start_camera
COPY ./default_camera.mpg /home/developer/default_camera.mpg

RUN chown -R developer.developer /home/developer

USER developer

WORKDIR /home/developer

CMD sh /home/developer/start_camera

