FROM ubuntu:jammy-20220428

ENV DEBIAN_FRONTEND noninteractive

RUN groupadd -g 5000 dev \
    && useradd -u 5000 -g 5000 -m -s /bin/bash dev

RUN apt update \
    && apt install -y \
        apt-transport-https \
        automake \
        autotools-dev \
        bindfs \
        binutils \
        curl \
        dpkg-dev \
        git \
        gpg \
        graphviz \
        jq \
        less \
        libedit-dev \
        libjemalloc-dev \
        libncurses-dev \
        libpcre2-dev \
        libtool \
        make \
        nano \
        netcat \
        pkg-config \
        python3 \
        python3-docutils \
        python3-sphinx \
        python3-venv \
        tar \
        telnet \
        unzip \
        wget \
    && apt clean \
    && rm -rf /var/lib/apt/lists/*

RUN cd /tmp \
    && wget --no-check-certificate https://varnish-cache.org/_downloads/varnish-7.4.0.tgz \
    && tar zxvf varnish-*.tgz \
    && rm -f varnish-*.tgz \
    && cd varnish-* \
    && ./autogen.sh \
    && ./configure \
    && make \
    && make PREFIX='/usr/local' install \
    && ldconfig

COPY ./docker-entrypoint.sh /
ENTRYPOINT ["/docker-entrypoint.sh"]
