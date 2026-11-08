FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ca-certificates \
    clang-18 \
    cmake \
    curl \
    file \
    git \
    libicu-dev \
    lld-18 \
    llvm-18 \
    llvm-18-tools \
    ninja-build \
    pkg-config \
    python3 \
    ripgrep \
    strace \
    unzip \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

RUN ln -sf /usr/bin/clang-18 /usr/local/bin/clang \
    && ln -sf /usr/bin/clang++-18 /usr/local/bin/clang++ \
    && ln -sf /usr/bin/ld.lld-18 /usr/local/bin/ld.lld \
    && ln -sf /usr/bin/ld64.lld-18 /usr/local/bin/ld64.lld \
    && ln -sf /usr/bin/llvm-mc-18 /usr/local/bin/llvm-mc \
    && ln -sf /usr/bin/llvm-lipo-18 /usr/local/bin/llvm-lipo

WORKDIR /work
