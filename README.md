# player.camera

This small daemon is a fork of the AOSP goldfish emulator camera
service, to split it into a client and a server who talk over TCP,
instead of qemu sockets which we cannot use.

It needs an additional adaptation of the sources on the VM side in
order to work.

The goal of this fork is to provide a ffmpeg-backed camera that can
play any kind of video file as a camera input, instead of webcams like
the android emulator does. It switches files by listening to remote
commands on an AMQP server.

# Building

Building this software requires a working C compiler, glib-2.0,
[rabbitmq-c](https://github.com/alanxz/rabbitmq-c/), and pthreads.

  
    make
  

# Running


This dæmon requires several env variables to be set:

Variable                 | Usage
---                      | ---
AIC_PLAYER_VM_HOST       | IP address of the android machine
AIC_PLAYER_VM_ID         | Virtual machine identifier
AIC_PLAYER_AMQP_HOST     | AMQP host IP
AIC_PLAYER_AMQP_USERNAME | Username for AMQP 
AIC_PLAYER_AMQP_PASSWORD | Password for AMQP 

# Updating the base sources

While clang-format was used on the sources, a special care was given to not
touch at all the biggest chunks of code from AOSP in order to easily diff
the new changes.

# License

Since it is a derivative of a part of GPLv2 software (qemu), this part is
also licenced under the GPLv2.
