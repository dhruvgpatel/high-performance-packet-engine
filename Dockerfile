# Multi-stage production build for High-Performance Network Packet Processing Engine
FROM ubuntu:22.04 AS builder

ENV DEBIAN_FRONTEND=noninteractive

# Install modern C++17 build toolchain
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    ninja-build \
    python3 \
    python3-pip \
    libpthread-stubs0-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . /app

# Build engine and unit tests
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=ON \
    && cmake --build build -j$(nproc)

# Run test suite during container verification
RUN ctest --test-dir build --output-on-failure

# Production runtime image
FROM ubuntu:22.04 AS runtime

ENV DEBIAN_FRONTEND=noninteractive
RUN apt-get update && apt-get install -y \
    python3 \
    python3-pip \
    iproute2 \
    net-tools \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=builder /app/build/packet_engine /app/bin/packet_engine
COPY --from=builder /app/build/packet_generator /app/bin/packet_generator
COPY --from=builder /app/config /app/config
COPY --from=builder /app/scripts /app/scripts

ENV PATH="/app/bin:${PATH}"

EXPOSE 9000/udp 9000/tcp 9002/tcp

ENTRYPOINT ["packet_engine"]
CMD ["--config", "/app/config/config.json"]
