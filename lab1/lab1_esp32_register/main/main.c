/*
 * İleri Gömülü Sistemler - Laboratuvar 1 (ESP32-S3)
 * GPIO4'e bağlı LED'in yalnızca registerlar kullanılarak sürülmesi.
 * Hiçbir ESP-IDF GPIO sürücü fonksiyonu kullanılmaz.
 */
#include <stdint.h>
#include <inttypes.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_timer.h"
#include "esp_log.h"

/* ---- Taban adresler (ESP32-S3 TRM, soc/reg_base.h) ---------------- */
#define DR_GPIO_BASE            0x60004000u
#define DR_IO_MUX_BASE          0x60009000u

/* ---- GPIO registerları (soc/gpio_reg.h) ---------------------------- */
#define GPIO_OUT_REG            (DR_GPIO_BASE + 0x0004u)
#define GPIO_OUT_W1TS_REG       (DR_GPIO_BASE + 0x0008u)
#define GPIO_OUT_W1TC_REG       (DR_GPIO_BASE + 0x000Cu)
#define GPIO_ENABLE_REG         (DR_GPIO_BASE + 0x0020u)
#define GPIO_ENABLE_W1TS_REG    (DR_GPIO_BASE + 0x0024u)
#define GPIO_ENABLE_W1TC_REG    (DR_GPIO_BASE + 0x0028u)
#define GPIO_IN_REG             (DR_GPIO_BASE + 0x003Cu)
#define GPIO_PIN_REG(n)         (DR_GPIO_BASE + 0x0074u + 4u * (n))
#define GPIO_FUNC_OUT_SEL_REG(n) (DR_GPIO_BASE + 0x0554u + 4u * (n))

/* ---- IO_MUX registerı (soc/io_mux_reg.h) --------------------------- */
#define IO_MUX_GPIO_REG(n)      (DR_IO_MUX_BASE + 0x0004u + 4u * (n))

/* ---- Bit alanları --------------------------------------------------- */
/* IO_MUX_GPIOn_REG */
#define FUN_PD                  (1u << 7)
#define FUN_PU                  (1u << 8)
#define FUN_IE                  (1u << 9)
#define FUN_DRV_S               10
#define FUN_DRV_M               (0x3u << FUN_DRV_S)
#define MCU_SEL_S               12
#define MCU_SEL_M               (0x7u << MCU_SEL_S)
#define PIN_FUNC_GPIO           1u
/* GPIO_PINn_REG */
#define PIN_PAD_DRIVER          (1u << 2)
#define PIN_INT_TYPE_M          (0x7u << 7)
#define PIN_INT_ENA_M           (0x1Fu << 13)
/* GPIO_FUNCn_OUT_SEL_CFG_REG */
#define SIG_GPIO_OUT_IDX        256u
#define FUNC_OEN_SEL            (1u << 10)

/* ---- Uygulama sabitleri -------------------------------------------- */
#define REG(addr)               (*(volatile uint32_t *)(addr))
#define LED_GPIO                4u
#define LED_MASK                (1u << LED_GPIO)
#define TOGGLE_COUNT            100000

static const char *TAG = "lab1";

static void led_init(void)
{
    /* Adım 1: pini güvenli duruma al (çıkış kapalı, düzey 0) */
    REG(GPIO_ENABLE_W1TC_REG) = LED_MASK;
    REG(GPIO_OUT_W1TC_REG)    = LED_MASK;

    /* Adım 2: IO_MUX - GPIO işlevi, orta sürme gücü, giriş tamponu açık */
    uint32_t mux = REG(IO_MUX_GPIO_REG(LED_GPIO));
    mux &= ~(MCU_SEL_M | FUN_DRV_M | FUN_IE | FUN_PU | FUN_PD);
    mux |= (PIN_FUNC_GPIO << MCU_SEL_S) | (2u << FUN_DRV_S) | FUN_IE;
    REG(IO_MUX_GPIO_REG(LED_GPIO)) = mux;

    /* Adım 3: GPIO_PIN4 - itme-çekme (push-pull) çıkış, kesme kapalı */
    uint32_t pin = REG(GPIO_PIN_REG(LED_GPIO));
    pin &= ~(PIN_PAD_DRIVER | PIN_INT_TYPE_M | PIN_INT_ENA_M);
    REG(GPIO_PIN_REG(LED_GPIO)) = pin;

    /* Adım 4: GPIO matrisi - pini GPIO_OUT sürer, çıkış izni GPIO_ENABLE'dan */
    REG(GPIO_FUNC_OUT_SEL_REG(LED_GPIO)) = SIG_GPIO_OUT_IDX | FUNC_OEN_SEL;

    /* Adım 5: çıkış sürücüsünü aç */
    REG(GPIO_ENABLE_W1TS_REG) = LED_MASK;
}

static void reg_dump(void)
{
    ESP_LOGI(TAG, "IO_MUX_GPIO4  = 0x%08" PRIx32, REG(IO_MUX_GPIO_REG(LED_GPIO)));
    ESP_LOGI(TAG, "GPIO_PIN4     = 0x%08" PRIx32, REG(GPIO_PIN_REG(LED_GPIO)));
    ESP_LOGI(TAG, "FUNC4_OUT_SEL = 0x%08" PRIx32, REG(GPIO_FUNC_OUT_SEL_REG(LED_GPIO)));
    ESP_LOGI(TAG, "GPIO_ENABLE   = 0x%08" PRIx32, REG(GPIO_ENABLE_REG));
    ESP_LOGI(TAG, "GPIO_OUT      = 0x%08" PRIx32, REG(GPIO_OUT_REG));
    ESP_LOGI(TAG, "GPIO_IN       = 0x%08" PRIx32, REG(GPIO_IN_REG));
}

static void blink(int count, int half_period_ms)
{
    for (int i = 0; i < count; i++) {
        REG(GPIO_OUT_W1TS_REG) = LED_MASK;
        vTaskDelay(pdMS_TO_TICKS(half_period_ms));
        REG(GPIO_OUT_W1TC_REG) = LED_MASK;
        vTaskDelay(pdMS_TO_TICKS(half_period_ms));
    }
}

static void toggle_burst(void)
{
    int64_t t0 = esp_timer_get_time();
    for (int i = 0; i < TOGGLE_COUNT; i++) {
        REG(GPIO_OUT_W1TS_REG) = LED_MASK;
        REG(GPIO_OUT_W1TC_REG) = LED_MASK;
    }
    int64_t dt_us = esp_timer_get_time() - t0;
    int64_t f_hz  = (int64_t)TOGGLE_COUNT * 1000000 / dt_us;
    ESP_LOGI(TAG, "%d periyot, %" PRId64 " us, ortalama f = %" PRId64 " Hz",
             TOGGLE_COUNT, dt_us, f_hz);
}

void app_main(void)
{
    led_init();
    reg_dump();

    REG(GPIO_OUT_W1TS_REG) = LED_MASK;
    ESP_LOGI(TAG, "LED yakıldı, GPIO_IN bit 4 = %" PRIu32,
             (REG(GPIO_IN_REG) >> LED_GPIO) & 1u);
    REG(GPIO_OUT_W1TC_REG) = LED_MASK;

    ESP_LOGI(TAG, "Bölüm A: 1 Hz yanıp sönme (5 kez)");
    blink(5, 500);

    ESP_LOGI(TAG, "Bölüm B: en yüksek hızda değiştirme");
    while (1) {
        toggle_burst();
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
