# syntax=docker/dockerfile:1.7

# ---------- builder ----------
FROM --platform=$BUILDPLATFORM ubuntu:22.04 AS builder

ARG DEBIAN_FRONTEND=noninteractive
ARG TZ=Etc/UTC
ARG TARGETARCH
ENV TZ=${TZ}

# Stabilne APT + komplet narzędzi do "depends"
RUN set -eux; \
    rm -rf /var/lib/apt/lists/*; \
    apt-get -o Acquire::Retries=5 update; \
    apt-get -o Dpkg::Use-Pty=0 -y --no-install-recommends install \
        ca-certificates \
        curl \
        git \
        pkg-config \
        build-essential \
        cmake \
        make \
        automake \
        libtool \
        autotools-dev \
        python3 \
        zip \
        unzip \
        xz-utils \
        bison \
        flex \
        file \
        patch; \
    # Cross-kompilator tylko dla arm64
    if [ "$TARGETARCH" = "arm64" ]; then \
        apt-get -y --no-install-recommends install \
            gcc-aarch64-linux-gnu g++-aarch64-linux-gnu; \
    fi; \
    rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .

# Budowa zależności i binarek (z rekursywnymi submodułami)
RUN set -eux; \
    git submodule update --init --recursive; \
    rm -rf build; \
    case "${TARGETARCH}" in \
      amd64)  HOST_TRIPLET=x86_64-linux-gnu ;; \
      arm64)  HOST_TRIPLET=aarch64-linux-gnu ;; \
      *) echo "Unsupported TARGETARCH: ${TARGETARCH}"; exit 1 ;; \
    esac; \
    make -j"$(nproc)" depends target="${HOST_TRIPLET}"; \
    mkdir -p /out/bin; \
    cp -a "build/${HOST_TRIPLET}/release/bin/." /out/bin/

# ---------- runtime ----------
FROM --platform=$TARGETPLATFORM ubuntu:22.04

ARG DEBIAN_FRONTEND=noninteractive

RUN set -eux; \
    rm -rf /var/lib/apt/lists/*; \
    apt-get -o Acquire::Retries=5 update; \
    apt-get -y --no-install-recommends install ca-certificates; \
    rm -rf /var/lib/apt/lists/*

COPY --from=builder /out/bin/ /usr/local/bin/

# Nie uruchamiaj demona jako root
RUN adduser --system --group --disabled-password arqma && \
    mkdir -p /wallet /home/arqma/.arqma && \
    chown -R arqma:arqma /home/arqma/.arqma /wallet

VOLUME /home/arqma/.arqma
VOLUME /wallet

EXPOSE 19993 19994 19995

USER arqma

ENTRYPOINT ["arqmad", "--p2p-bind-ip=0.0.0.0", "--p2p-bind-port=19993", "--rpc-bind-ip=0.0.0.0", "--rpc-bind-port=19994", "--restricted-rpc", "--zmq-rpc-bind-ip=0.0.0.0", "--zmq-rpc-bind-port=19995" --non-interactive", "--confirm-external-bind"]
