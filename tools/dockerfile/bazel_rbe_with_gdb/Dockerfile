FROM gcr.io/cloud-marketplace/google/rbe-ubuntu16-04@sha256:f6568d8168b14aafd1b707019927a63c2d37113a03bcee188218f99bd0327ea1

RUN apt-get update -y && \
      apt-get install -y gdb sudo
RUN echo "nobody ALL = NOPASSWD: /usr/bin/gdb" >>/etc/sudoers
