"""Lab 1 grafikleri (Gamze Şirin).

    cd lab1 && python3 grafik.py [pi_kayit_dosyasi]

Girdi : olcum/esp32_register_kayit.txt  (ESP32 monitör kaydı)
        olcum/pi_olcum.txt              (pi_gpio çıktıları)
Çıktı : olcum/sekil1.png
"""
import re
import statistics as st
import sys
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

OLCUM = Path(__file__).resolve().parent / "olcum"
RENK_ESP, RENK_NORMAL, RENK_RT = "#2a78d6", "#eb6834", "#1baf7a"
METIN, SOLUK, IZGARA = "#0b0b0b", "#52514e", "#dcdad6"


def esp32_sureleri():
    sureler = []
    for satir in (OLCUM / "esp32_register_kayit.txt").read_text(encoding="utf-8").splitlines():
        m = re.search(r"100000 periyot, (\d+) us", satir)
        if m:
            sureler.append(int(m.group(1)))
    return sureler


def pi_kayitlari(dosya):
    """pi_gpio çıktısını {'normal'|'rt': {'f': [...], 'hist': [(alt, ust, sayi)], 'yuzde': {...}}} yapar."""
    kayit, aktif = {}, None
    for satir in dosya.read_text(encoding="utf-8").splitlines():
        if satir.startswith("pi_gpio |"):
            aktif = "rt" if "SCHED_FIFO" in satir else "normal"
            kayit.setdefault(aktif, {"f": [], "hist": [], "yuzde": {}})
            continue
        if aktif is None:
            continue
        m = re.search(r"pencere \d+: .* f = (\d+) Hz", satir)
        if m:
            kayit[aktif]["f"].append(int(m.group(1)))
        m = re.search(r"([\d.]+) - +([\d.]+) us : +(\d+)", satir)
        if m:
            kayit[aktif]["hist"].append((float(m.group(1)), float(m.group(2)), int(m.group(3))))
        m = re.search(r"([\d.]+) us ve üstü +: +(\d+)", satir)
        if m:
            kayit[aktif]["hist"].append((float(m.group(1)), None, int(m.group(2))))
        for ad, desen in [("p50", r"%50 (\d+)"), ("p99", r"%99 (\d+)"),
                          ("p999", r"%99,9 (\d+)"), ("max", r"en uzun (\d+)")]:
            m = re.search(desen + r" ns", satir)
            if m:
                kayit[aktif]["yuzde"][ad] = int(m.group(1))
    return kayit


def main():
    pi_dosya = Path(sys.argv[1]) if len(sys.argv) > 1 else OLCUM / "pi_olcum.txt"
    esp = esp32_sureleri()
    pi = pi_kayitlari(pi_dosya)

    print(f"ESP32: {len(esp)} ölçüm, {min(esp)}–{max(esp)} us, ort {st.mean(esp):.3f} us, "
          f"f = {1e11 / st.mean(esp):.0f} Hz")
    for ad, v in pi.items():
        print(f"Pi {ad}: f = {st.mean(v['f']):.0f} Hz ({min(v['f'])}–{max(v['f'])}), yüzdelik {v['yuzde']}")

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 8,
                         "axes.edgecolor": SOLUK, "xtick.color": SOLUK, "ytick.color": SOLUK})
    fig, (sol, sag) = plt.subplots(1, 2, figsize=(7.2, 2.6), dpi=220,
                                   gridspec_kw={"width_ratios": [1, 1.5]})

    # (a) ESP32: 100 000 periyotluk ölçümlerin süre dağılımı
    degerler = list(range(min(esp), max(esp) + 1))
    sayilar = [esp.count(d) for d in degerler]
    sol.bar(range(len(degerler)), sayilar, width=0.55, color=RENK_ESP)
    for i, s in enumerate(sayilar):
        sol.text(i, s + 3, str(s), ha="center", va="bottom", fontsize=7.5, color=METIN)
    sol.set_xticks(range(len(degerler)))
    sol.set_xticklabels([f"{d:,}".replace(",", " ") for d in degerler])
    sol.set_ylim(0, max(sayilar) * 1.18)
    sol.set_xlabel("100 000 periyodun süresi (µs)")
    sol.set_ylabel("Ölçüm sayısı")
    sol.set_title(f"(a) ESP32-S3: {len(esp)} ölçümün dağılımı", loc="left", fontsize=8.5, color=METIN)

    # (b) Pi: periyot histogramı
    hist_n = pi["normal"]["hist"]
    etiketler = [(f"{a:g}–{u:g}" if u is not None else f"≥{a:g}").replace(".", ",") for a, u, _ in hist_n]
    x = range(len(etiketler))
    genislik = 0.4
    for kayma, ad, renk, yazi in [(-genislik / 2, "normal", RENK_NORMAL, "normal"),
                                  (genislik / 2, "rt", RENK_RT, "SCHED_FIFO 90, CPU 2")]:
        if ad not in pi:
            continue
        sayilar = [max(s, 0.6) for _, _, s in pi[ad]["hist"]]  # 0 kutular log eksende görünmez
        sag.bar([i + kayma for i in x], sayilar, width=genislik, color=renk, label=yazi)
    sag.set_yscale("log")
    sag.set_ylim(0.8, 2e6)
    sag.set_xticks(list(x))
    sag.set_xticklabels(etiketler, rotation=55, ha="right", fontsize=6.5)
    sag.set_xlabel("Periyot aralığı (µs)")
    sag.set_ylabel("Periyot sayısı (log)")
    sag.set_title("(b) Raspberry Pi 4: periyot histogramı", loc="left", fontsize=8.5, color=METIN)
    sag.legend(frameon=False, fontsize=7, loc="upper right")

    for eksen in (sol, sag):
        eksen.grid(axis="y", color=IZGARA, linewidth=0.6)
        eksen.set_axisbelow(True)
        eksen.spines["top"].set_visible(False)
        eksen.spines["right"].set_visible(False)

    fig.tight_layout(w_pad=1.8)
    fig.savefig(OLCUM / "sekil1.png", facecolor="white")
    print("Kaydedildi:", OLCUM / "sekil1.png")


if __name__ == "__main__":
    main()
