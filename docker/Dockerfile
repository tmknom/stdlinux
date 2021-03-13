FROM debian:buster

RUN apt-get update -y \
    && apt-get install -y --no-install-recommends \
    build-essential gdb strace man manpages-dev vim less procps psmisc lsof curl \
    && apt-get clean -y \
    && rm -rf /var/lib/apt/lists/*
