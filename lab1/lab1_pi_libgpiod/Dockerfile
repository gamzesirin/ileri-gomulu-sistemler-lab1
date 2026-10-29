# Mac'te Raspberry Pi 4 için çapraz derleme ortamı.
# Pi ile aynı Debian sürümü kullanılır (Pi'de: grep VERSION_CODENAME /etc/os-release).
ARG SURUM=trixie
FROM debian:${SURUM}
# arm64 libgpiod ve libc, Debian'ın çoklu mimari desteğiyle kurulur (Pi'nin kütüphanelerinin kopyası).
RUN dpkg --add-architecture arm64 \
 && apt-get update \
 && apt-get install -y --no-install-recommends crossbuild-essential-arm64 libgpiod-dev:arm64 make file \
 && rm -rf /var/lib/apt/lists/*
WORKDIR /proje
