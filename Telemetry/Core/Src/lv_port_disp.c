#include "lv_port_disp.h"
#include "stm32f7xx_hal.h"

extern LTDC_HandleTypeDef hltdc;

// v9'da flush fonksiyonunun imzası değişti
static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map);

void lv_port_disp_init(void)
{
    // 1. Ekran objesini oluştur (480x272 çözünürlük)
    lv_display_t * disp = lv_display_create(480, 272);

    // 2. Renk formatını RGB565 (16-bit) olarak ayarla
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

    // 3. Çizim tamponu (Buffer) - RGB565 = 2 byte/piksel
    //    1/10 ekran yüksekliği = 27 satır
    static uint8_t buf1[480 * 27 * 2]; // ~25KB (RGB565)
    lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

    // 4. Ekrana yazma fonksiyonunu (flush_cb) tanıt
    lv_display_set_flush_cb(disp, disp_flush);
}

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map)
{
    // RGB565: 16-bit (2 byte) piksel formatı
    uint16_t * color_p = (uint16_t *)px_map;
    uint16_t * fb = (uint16_t *)0xC0000000;  // SDRAM framebuffer

    // Pikselleri fiziksel framebuffer'a kopyala
    for(int32_t y = area->y1; y <= area->y2; y++) {
        for(int32_t x = area->x1; x <= area->x2; x++) {
            fb[y * 480 + x] = *color_p;
            color_p++;
        }
    }

    // LVGL'ye çizimin bittiğini haber ver
    lv_display_flush_ready(disp);
}