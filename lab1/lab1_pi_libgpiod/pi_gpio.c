/*
 * Laboratuvar 1 - Raspberry Pi 4
 * GPIO17'ye (fiziksel pin 11) bağlı LED'i libgpiod v2 ile sürer ve
 * pinin ne kadar hızlı değiştirilebildiğini ölçer.
 *
 * Devre: pin 11 (GPIO17) -> direnç -> LED (+), LED (-) -> pin 9 (GND)
 *
 *   ./pi_gpio yak        LED'i 1 Hz'de 5 kez yakıp söndürür
 *   ./pi_gpio frekans    1 saniyelik 5 pencerede yapılan periyot sayısını sayar
 *   ./pi_gpio dagilim    her periyodun süresini ölçer, histogram ve yüzdelikler
 *
 * Seçenekler:
 *   -r      gerçek zamanlı çalış (SCHED_FIFO 90 + mlockall, sudo gerekir)
 *   -c N    işlemi N numaralı çekirdeğe sabitle
 *   -n N    dagilim modunda ölçülecek periyot sayısı (varsayılan 1000000)
 *
 * Mac'te Docker ile çapraz derlenir: ./capraz_derle.sh
 */
#define _GNU_SOURCE
#include <gpiod.h>
#include <math.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define YONGA_YOLU   "/dev/gpiochip0"
#define LED_HATTI    17u
#define PENCERE_NS   1000000000ull   /* frekans modu: 1 s */
#define PENCERE_SAY  5
#define BLOK         1000            /* saat bu kadar periyotta bir okunur */

static struct gpiod_chip *yonga;
static struct gpiod_line_request *istek;
static volatile sig_atomic_t durdur;

static void sinyal_geldi(int s) { (void)s; durdur = 1; }

static uint64_t simdi(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

static inline void pin_yaz(int deger)
{
    gpiod_line_request_set_value(istek, LED_HATTI,
                                 deger ? GPIOD_LINE_VALUE_ACTIVE
                                       : GPIOD_LINE_VALUE_INACTIVE);
}

/* GPIO17'yi çıkış olarak ister; başarısızsa -1 döner */
static int hatti_al(void)
{
    yonga = gpiod_chip_open(YONGA_YOLU);
    if (yonga == NULL) {
        perror(YONGA_YOLU);
        return -1;
    }

    struct gpiod_line_settings *ayar = gpiod_line_settings_new();
    struct gpiod_line_config *hatlar = gpiod_line_config_new();
    struct gpiod_request_config *talep = gpiod_request_config_new();
    unsigned int hat = LED_HATTI;
    int sonuc = -1;

    if (ayar && hatlar && talep) {
        gpiod_line_settings_set_direction(ayar, GPIOD_LINE_DIRECTION_OUTPUT);
        gpiod_line_settings_set_output_value(ayar, GPIOD_LINE_VALUE_INACTIVE);
        gpiod_line_config_add_line_settings(hatlar, &hat, 1, ayar);
        gpiod_request_config_set_consumer(talep, "gamze_lab1");
        istek = gpiod_chip_request_lines(yonga, talep, hatlar);
        if (istek != NULL)
            sonuc = 0;
        else
            perror("gpiod_chip_request_lines");
    }
    gpiod_request_config_free(talep);
    gpiod_line_config_free(hatlar);
    gpiod_line_settings_free(ayar);
    if (sonuc < 0)
        gpiod_chip_close(yonga);
    return sonuc;
}

static void hatti_birak(void)
{
    pin_yaz(0);
    gpiod_line_request_release(istek);
    gpiod_chip_close(yonga);
}

/* ---- yak: 1 Hz, 5 kez -------------------------------------------- */
static void led_yak_son(void)
{
    const struct timespec yarim = { 0, 500000000L };
    printf("LED 1 Hz ile 5 kez yakılıp söndürülüyor\n");
    for (int i = 1; i <= 5 && !durdur; i++) {
        pin_yaz(1);
        nanosleep(&yarim, NULL);
        pin_yaz(0);
        nanosleep(&yarim, NULL);
        printf("  %d. yanıp sönme tamam\n", i);
        fflush(stdout);
    }
}

/* ---- frekans: sabit sürede kaç periyot yapılıyor -------------------- */
static void frekans_olc(void)
{
    double toplam = 0, en_kucuk = 1e18, en_buyuk = 0;
    int pencere;

    printf("En yüksek hız: %d pencere x %.1f s\n", PENCERE_SAY, PENCERE_NS / 1e9);
    for (pencere = 1; pencere <= PENCERE_SAY && !durdur; pencere++) {
        uint64_t periyot = 0, baslangic = simdi(), gecen;
        do {
            for (int i = 0; i < BLOK; i++) {
                pin_yaz(1);
                pin_yaz(0);
            }
            periyot += BLOK;
            gecen = simdi() - baslangic;
        } while (gecen < PENCERE_NS);

        double f = periyot * 1e9 / (double)gecen;
        printf("  pencere %d: %llu periyot / %.4f s -> f = %.0f Hz (periyot %.1f ns)\n",
               pencere, (unsigned long long)periyot, gecen / 1e9, f, gecen / (double)periyot);
        toplam += f;
        if (f < en_kucuk) en_kucuk = f;
        if (f > en_buyuk) en_buyuk = f;
    }
    int n = pencere - 1;
    if (n > 0)
        printf("Sonuç: ortalama f = %.0f Hz, en düşük %.0f Hz, en yüksek %.0f Hz, fark %%%.2f\n",
               toplam / n, en_kucuk, en_buyuk, 100.0 * (en_buyuk - en_kucuk) / en_buyuk);
}

/* ---- dagilim: her periyot ayrı ölçülür ------------------------------ */
static int kucukten_buyuge(const void *a, const void *b)
{
    uint32_t x = *(const uint32_t *)a, y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

static void dagilim_olc(long adet)
{
    /* histogram sınırları (ns); son kutu "200 µs ve üstü" */
    static const uint32_t sinir[] = { 1800, 1900, 2000, 2100, 2200, 2500, 3000,
                                      5000, 10000, 20000, 50000, 100000, 200000 };
    enum { KUTU = sizeof(sinir) / sizeof(sinir[0]) + 1 };
    long kutu[KUTU] = { 0 };

    uint32_t *sure = calloc((size_t)adet, sizeof(uint32_t));
    if (sure == NULL) {
        perror("calloc");
        return;
    }
    printf("Periyot dağılımı: %ld periyot ölçülüyor (her periyoda bir saat okuması eklenir)\n", adet);

    uint64_t onceki = simdi();
    long i;
    for (i = 0; i < adet && !durdur; i++) {
        pin_yaz(1);
        pin_yaz(0);
        uint64_t t = simdi();
        sure[i] = (uint32_t)(t - onceki);
        onceki = t;
    }
    adet = i;

    double ort = 0, kare = 0;
    for (i = 0; i < adet; i++) {
        ort += sure[i];
        kare += (double)sure[i] * sure[i];
        int k = 0;
        while (k < KUTU - 1 && sure[i] >= sinir[k])
            k++;
        kutu[k]++;
    }
    ort /= adet;
    double ss = sqrt(fmax(kare / adet - ort * ort, 0));

    qsort(sure, (size_t)adet, sizeof(uint32_t), kucukten_buyuge);
    printf("  ortalama %.1f ns, standart sapma %.1f ns\n", ort, ss);
    printf("  en kısa %u ns | %%50 %u ns | %%90 %u ns | %%99 %u ns | %%99,9 %u ns | %%99,99 %u ns | en uzun %u ns\n",
           sure[0], sure[adet / 2], sure[(long)(adet * 0.90)], sure[(long)(adet * 0.99)],
           sure[(long)(adet * 0.999)], sure[(long)(adet * 0.9999)], sure[adet - 1]);

    printf("  Histogram:\n");
    for (int k = 0; k < KUTU; k++) {
        double alt = k ? sinir[k - 1] / 1000.0 : 0;
        if (k < KUTU - 1)
            printf("    %7.2f - %7.2f us : %8ld  (%%%.4f)\n", alt, sinir[k] / 1000.0,
                   kutu[k], 100.0 * kutu[k] / adet);
        else
            printf("    %7.2f us ve üstü  : %8ld  (%%%.4f)\n", alt, kutu[k], 100.0 * kutu[k] / adet);
    }
    free(sure);
}

int main(int argc, char **argv)
{
    int gercek_zamanli = 0, cekirdek = -1, secenek;
    long adet = 1000000;

    while ((secenek = getopt(argc, argv, "rc:n:")) != -1) {
        if (secenek == 'r')      gercek_zamanli = 1;
        else if (secenek == 'c') cekirdek = atoi(optarg);
        else if (secenek == 'n') adet = atol(optarg);
        else {
            fprintf(stderr, "Kullanım: %s [-r] [-c N] [-n N] yak|frekans|dagilim\n", argv[0]);
            return 1;
        }
    }
    const char *mod = optind < argc ? argv[optind] : "yak";
    if (adet < 2) adet = 2;

    signal(SIGINT, sinyal_geldi);
    signal(SIGTERM, sinyal_geldi);

    if (cekirdek >= 0) {
        cpu_set_t kume;
        CPU_ZERO(&kume);
        CPU_SET(cekirdek, &kume);
        if (sched_setaffinity(0, sizeof(kume), &kume) != 0)
            perror("sched_setaffinity");
    }
    if (gercek_zamanli) {
        struct sched_param p = { .sched_priority = 90 };
        if (sched_setscheduler(0, SCHED_FIFO, &p) != 0) {
            perror("sched_setscheduler (sudo ile çalıştırın)");
            gercek_zamanli = 0;
        }
        if (mlockall(MCL_CURRENT | MCL_FUTURE) != 0)
            perror("mlockall");
    }

    if (hatti_al() != 0)
        return 1;

    char cekirdek_yazi[16] = "serbest";
    if (cekirdek >= 0)
        snprintf(cekirdek_yazi, sizeof(cekirdek_yazi), "CPU %d", cekirdek);
    printf("pi_gpio | libgpiod %s | %s hat %u | zamanlama: %s | çekirdek: %s\n",
           gpiod_api_version(), YONGA_YOLU, LED_HATTI,
           gercek_zamanli ? "SCHED_FIFO 90" : "normal (SCHED_OTHER)", cekirdek_yazi);

    if (strcmp(mod, "frekans") == 0)
        frekans_olc();
    else if (strcmp(mod, "dagilim") == 0)
        dagilim_olc(adet);
    else
        led_yak_son();

    hatti_birak();
    return 0;
}
