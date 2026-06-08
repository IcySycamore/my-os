# MY-OS RISCV Build Environment
# docker build -t myos-build .
# docker run --rm -v "$(pwd):/src" -w /src myos-build make

FROM ubuntu:22.04

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    gcc-riscv64-unknown-elf \
    qemu-system-misc \
    ca-certificates \
    && rm -rf /var/lib/apt/lists/*

CMD ["sh", "-c", "make && echo ===== BUILD OK ====="]
