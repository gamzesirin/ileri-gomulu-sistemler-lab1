#!/bin/sh
# Mac'te (x86-64 Debian konteynerinde) Raspberry Pi 4 için pi_gpio programını derler.
# Kullanım: ./capraz_derle.sh          (Pi'de Raspberry Pi OS trixie)
set -eu
cd "$(dirname "$0")"
SURUM="${1:-trixie}"
GORUNTU="gamze-pi-capraz:${SURUM}"

echo ">> Derleme ortamı hazırlanıyor (${SURUM}, linux/amd64)"
docker build --platform linux/amd64 --build-arg SURUM="${SURUM}" -t "${GORUNTU}" -f Dockerfile .

echo ">> pi_gpio derleniyor"
docker run --rm --platform linux/amd64 -v "$PWD":/proje -w /proje "${GORUNTU}" \
    sh -c 'make temizle pi_gpio && file pi_gpio && aarch64-linux-gnu-gcc --version | head -1'
