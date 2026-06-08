# MY-OS RISCV Build Environment
# docker build -t myos-build .
# docker run --rm -v "$(pwd):/src" -w /src myos-build make

FROM ubuntu:22.04

# 使用清华镜像源（国内加速）
RUN sed -i 's|http://archive.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list && \
    sed -i 's|http://security.ubuntu.com|https://mirrors.tuna.tsinghua.edu.cn|g' /etc/apt/sources.list

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-riscv64-unknown-elf \
    qemu-system-misc \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

CMD ["sh", "-c", "make && echo ===== BUILD OK ====="]
