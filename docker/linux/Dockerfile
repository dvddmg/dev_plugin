FROM --platform=linux/amd64 ubuntu:22.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y \
        build-essential pkg-config \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
        libgl1-mesa-dev libdbus-1-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /work