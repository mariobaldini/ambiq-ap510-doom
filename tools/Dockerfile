FROM ubuntu:24.04
# image-epoch: 2026-11-15  (the image tag is this file's hash; bump the date to force a rebuild)

ENV DEBIAN_FRONTEND=noninteractive

ARG JLINK_VERSION=V994
ARG JLINK_SHA256=260a1816af070f24b119c1898197dd4e8f6ddda11a584624a7d554196ea26479

# The J-Link postinst runs udevadm, which fails without udev in a container; the host owns the udev rule.
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    git \
    make \
    python3 \
    xz-utils \
 && curl -fSL --retry 3 --connect-timeout 30 \
    -d accept_license_agreement=accepted -d submit="Download software" \
    -o /tmp/jlink.deb "https://www.segger.com/downloads/jlink/JLink_Linux_${JLINK_VERSION}_x86_64.deb" \
 && echo "${JLINK_SHA256}  /tmp/jlink.deb" | sha256sum -c - \
 && dpkg --unpack /tmp/jlink.deb \
 && rm -f /var/lib/dpkg/info/jlink*.postinst \
 && apt-get -f install -y --no-install-recommends \
 && rm -f /tmp/jlink.deb \
 && apt-get clean && rm -rf /var/lib/apt/lists/*

RUN git config --system --add safe.directory '*' && command -v JLinkExe

WORKDIR /work
