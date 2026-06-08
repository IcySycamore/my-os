# MY-OS RISCV Build Environment
# docker build -t myos-build .
# docker run --rm -v "$(pwd):/src" -w /src myos-build make

FROM ubuntu:22.04

# Step 1: install ca-certificates from default archive first
# (needed before switching to any HTTPS mirror)
RUN apt-get update && apt-get install -y --no-install-recommends ca-certificates && rm -rf /var/lib/apt/lists/*

# Step 2: switch to Tsinghua mirror for faster China access
RUN sed -i 's|http://archive.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list

# Step 3: install build toolchain
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-riscv64-unknown-elf \
    qemu-system-misc \
    && rm -rf /var/lib/apt/lists/*

CMD ["sh", "-c", "make && echo ===== BUILD OK ====="]
