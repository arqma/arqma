# Multistage docker build, requires docker 17.05

# builder stage
FROM ubuntu:20.04 AS builder

RUN set -ex && \
    apt-get update && \
    DEBIAN_FRONTEND=noninteractive apt-get --no-install-recommends --yes install \
        build-essential \
        ca-certificates \
        cmake \
        curl \
        git \
        pkg-config

WORKDIR /src
COPY . .

ARG NPROC
RUN set -ex && \
    git submodule init && git submodule update && \
    rm -rf build && \
    if [ -z "$NPROC" ] ; \
    then make -j$(nproc) depends target=x86_64-linux-gnu ; \
    else make -j$NPROC depends target=x86_64-linux-gnu ; \
    fi

# runtime stage
FROM ubuntu:20.04

RUN set -ex && \
    apt-get update && \
    apt-get --no-install-recommends --yes install ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt
COPY --from=builder /src/build/x86_64-linux-gnu/release/bin /usr/local/bin/

# Below command is creating Arqma user to do not run daemon as a root
RUN adduser --system --group --disabled-password arqma && \
	mkdir -p /wallet /home/arqma/.arqma && \
	chown -R arqma:arqma /home/arqma/.arqma && \
	chown -R arqma:arqma /wallet

# Contains the blockchain
VOLUME /home/arqma/.arqma

# Generate your wallet via accessing the container and run:
# cd /wallet
# arqma-wallet-cli
VOLUME /wallet

EXPOSE 19993
EXPOSE 19994

# switch to user arqma
USER arqma

ENTRYPOINT ["arqmad", "--p2p-bind-ip=0.0.0.0", "--p2p-bind-port=19993", "--rpc-bind-ip=0.0.0.0", "--rpc-bind-port=19994", "--non-interactive", "--confirm-external-bind"]
