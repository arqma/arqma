# Multistage docker build, requires docker 17.05

# builder stage
FROM ubuntu:18.04 as builder

RUN set -ex && \
    apt-get update -y && \
    apt-get upgrade -y && \
    apt-get --no-install-recommends --yes install \
        ca-certificates \
        cmake \
        g++ \
        make \
        pkg-config \
        graphviz \
        doxygen \
        git \
        curl \
        libtool-bin \
        autoconf \
        automake \
        bzip2 \
        xsltproc \
        gperf \
        unzip \
    	docbook-xsl

WORKDIR /usr/local

ENV CFLAGS='-fPIC'
ENV CXXFLAGS='-fPIC'

# OpenSSL
ARG OPENSSL_VERSION=1.1.1k
ARG OPENSSL_HASH=892a0875b9872acd04a9fde79b1f943075d5ea162415de3047c327df33fbaee5
RUN set -ex \
    && curl -s -O https://www.openssl.org/source/openssl-${OPENSSL_VERSION}.tar.gz \
    && echo "${OPENSSL_HASH}  openssl-${OPENSSL_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf openssl-${OPENSSL_VERSION}.tar.gz \
    && cd openssl-${OPENSSL_VERSION} \
    && ./Configure linux-x86_64 no-shared --static "$CFLAGS" \
    && if [ -z "$NPROC" ] ; \
    then make build_generated -j$(nproc) ; \
    else make build_generated -j$NPROC ; \
    fi \
    && if [ -z "$NPROC" ] ; \
    then make libcrypto.a -j$(nproc) ; \
    else make libcrypto.a -j$NPROC ; \
    fi \
    && make install
ENV OPENSSL_ROOT_DIR=/usr/local/openssl-${OPENSSL_VERSION}

#Cmake
ARG CMAKE_VERSION=3.20.0
ARG CMAKE_VERSION_DOT=v3.20
ARG CMAKE_HASH=9c06b2ddf7c337e31d8201f6ebcd3bba86a9a033976a9aee207fe0c6971f4755
RUN set -ex \
    && curl -s -O https://cmake.org/files/${CMAKE_VERSION_DOT}/cmake-${CMAKE_VERSION}.tar.gz \
    && echo "${CMAKE_HASH}  cmake-${CMAKE_VERSION}.tar.gz" | sha256sum -c \
    && tar -xzf cmake-${CMAKE_VERSION}.tar.gz \
    && cd cmake-${CMAKE_VERSION} \
    && ./configure \
    && if [ -z "$NPROC" ] ; \
    then make -j$(nproc) ; \
    else make -j$NPROC ; \
    fi \
    && make install

WORKDIR /src
COPY . .

ENV USE_SINGLE_BUILDDIR=1
ARG NPROC
RUN set -ex && \
    git submodule init && \
    rm -rf build && \
    if [ -z "$NPROC" ] ; \
    then make -j$(nproc) depends target=x86_64-linux-gnu ; \
    else make -j$NPROC depends target=x86_64-linux-gnu ; \
    fi

# runtime stage
FROM ubuntu:18.04

RUN set -ex && \
    apt-get update && \
    apt-get --no-install-recommends --yes install ca-certificates && \
    apt-get clean && \
    rm -rf /var/lib/apt

COPY --from=builder /src/build/x86_64-linux-gnu/release/bin /usr/local/bin/
