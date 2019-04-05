FROM debian:buster

RUN apt-get update -qq && apt-get -y --no-install-recommends install \
       sudo \
       wget \
       ca-certificates \
       python3-pip \
       xz-utils \
       git \
       cmake \
       ninja-build \
       gperf \
       ccache \
       doxygen \
       dfu-util \
       device-tree-compiler \
       python3-ply \
       python3-pip \
       python3-setuptools \
       python3-wheel \
       xz-utils \
       file \
       make \
       gcc-multilib \
       autoconf \
       automake \
       libtool \
       librsvg2-bin \
       texlive-latex-base \
       texlive-latex-extra \
       latexmk \
       texlive-fonts-recommended \
       gcc-arm-none-eabi \
       qemu-system-arm \
    && rm -rf /var/lib/apt/lists/*

# Appease Python
ENV LANG C.UTF-8

COPY scripts/requirements.txt /tmp
RUN adduser --uid 1000 --system --group --shell /bin/bash builder \
        && mkdir /home/builder/bin
USER builder

RUN pip3 install --user -r /tmp/requirements.txt
RUN pip3 install --user pyyaml
