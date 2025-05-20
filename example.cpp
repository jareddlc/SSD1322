// This is an example of how to use this SSD1322 driver with LVGL and the ESP32 framework.
// The two important functions needed are ssd1322_lvgl_flush and ssd1322_lvgl_align_area.
// These functions are for telling LVGL how to write to the display correctly.
// SSD1322
#include <ssd1322.h>

// std
#include <stdio.h>

// esp32
#include <esp_timer.h>

// graphics
#include <lvgl.h>

constexpr int SCREEN_WIDTH = 256;
constexpr int SCREEN_HEIGHT = 64;
int LV_TICK_PERIOD_MS = 1;

static SSD1322 oled{GPIO_NUM_17, GPIO_NUM_16, GPIO_NUM_5, GPIO_NUM_18, GPIO_NUM_23, VSPI_HOST, false}; // ESP32 DevKitC v4
// static SSD1322 oled{GPIO_NUM_10, GPIO_NUM_6, GPIO_NUM_7, GPIO_NUM_12, GPIO_NUM_11, SPI2_HOST}; // ESP32S3 DevKitC

// display
static lv_disp_t *disp;
// static constexpr uint32_t disp_buff_size = (SCREEN_WIDTH * (SCREEN_HEIGHT >> 3));
// static uint8_t *disp_buff_1;
// static uint8_t *disp_buff_2;
// static constexpr uint32_t pixel_buff_size = disp_buff_size >> 1;
// static uint8_t *pixel_buff;

static constexpr uint32_t disp_buff_size = (SCREEN_WIDTH * (SCREEN_HEIGHT >> 1));
static DRAM_ATTR uint8_t disp_buff_1[disp_buff_size] = {};
static DRAM_ATTR uint8_t disp_buff_2[disp_buff_size] = {};
static constexpr uint32_t pixel_buff_size = disp_buff_size >> 1;
static DRAM_ATTR uint8_t pixel_buff[pixel_buff_size] = {};

// You need to call the lv_tick_inc(tick_period) function periodically and provide the call period in milliseconds.
// For example, lv_tick_inc(1) when calling every millisecond.
static void lv_tick_task(void *arg) {
  (void)arg;
  lv_tick_inc(LV_TICK_PERIOD_MS);
}

static void IRAM_ATTR ssd1322_lvgl_flush(lv_display_t *display, const lv_area_t *area, uint8_t *px_map) {
  uint8_t *buf = px_map;
  uint16_t pixels = ((area->x2 - area->x1) + 1) * ((area->y2 - area->y1) + 1); // why +1?
  uint16_t bytes = pixels >> 1;
  // uint16_t bytes = (pixels + 1) >> 1;  // add 1 to ensure even for odd pixels

  oled.set_column_address(0x1C + (area->x1 / 4), 0x1C + (area->x2 / 4));
  oled.set_row_address(area->y1, area->y2);
  oled.set_write_ram();

  for (uint16_t x = 0; x < pixels; x++) {
    uint16_t z = x >> 1;              // each two pixels go into one byte
    uint8_t pixel_4bit = buf[x] >> 4; // convert 8-bit (0-255) to 4-bit (0-15)

    // if x is odd
    if (x & 1u) {
      pixel_buff[z] |= pixel_4bit;        // store lower 4 bits
    } else {
      pixel_buff[z] = (pixel_4bit << 4);  // store upper 4 bits
    }
  }

  // Polling transaction
  oled.send_ssd1322_data_buffer(pixel_buff, bytes);
  lv_display_flush_ready(display);

  // Async transaction
  // oled.send_ssd1322_data_buffer_async(pixel_buff, bytes, display);
}

static void IRAM_ATTR ssd1322_lvgl_align_area(lv_event_t *e) {
  lv_area_t *area = (lv_area_t *)lv_event_get_param(e);

  // ensure area aligns to multiples of 4 pixels horizontally
  area->x1 &= ~3;                       // align left to 4-pixel boundary
  area->x2 = ((area->x2 + 4) & ~3) - 1; // align right and subtract 1 for inclusive range
}

extern "C" void app_main(void) {
  // init OLED
  oled.init(SCREEN_WIDTH, SCREEN_HEIGHT);

  // init lvgl
  lv_init();

  // configure display and set render callbacks
  disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
  lv_display_set_flush_cb(disp, ssd1322_lvgl_flush);
  lv_display_add_event_cb(disp, ssd1322_lvgl_align_area, LV_EVENT_INVALIDATE_AREA, nullptr);

  // allocate double buffers with DMA capability
  // disp_buff_1 = (uint8_t *)heap_caps_aligned_alloc(4, disp_buff_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  // disp_buff_2 = (uint8_t *)heap_caps_aligned_alloc(4, disp_buff_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  // pixel_buff = (uint8_t *)heap_caps_aligned_alloc(4, pixel_buff_size, MALLOC_CAP_DMA | MALLOC_CAP_8BIT);
  lv_display_set_buffers(disp, disp_buff_1, disp_buff_2, disp_buff_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_color_format(disp, LV_COLOR_FORMAT_L8); // IMPORTANT
  // lv_display_set_color_format(disp, LV_COLOR_FORMAT_I4); // IMPORTANT

  // create timer
  esp_timer_handle_t timer_handle;
  esp_timer_create_args_t timer_args = {
    .callback = lv_tick_task,
    .arg = nullptr,
    .dispatch_method = ESP_TIMER_TASK,
    .name = "lv_tick_timer",
    .skip_unhandled_events = false
  };
  esp_timer_create(&timer_args, &timer_handle);
  esp_timer_start_periodic(timer_handle, LV_TICK_PERIOD_MS * 1000);

  // set display background color.
  lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), LV_PART_MAIN);
  lv_obj_set_scrollbar_mode(lv_screen_active(), LV_SCROLLBAR_MODE_OFF);

  // oled.test();

  // lvgl code
  static lv_style_t style_bar;
  static lv_style_t style_bar_indic;
  static lv_obj_t *bar;

  lv_style_init(&style_bar);
  lv_style_set_border_color(&style_bar, lv_color_white());
  lv_style_set_border_width(&style_bar, 1);
  lv_style_set_pad_all(&style_bar, 1);
  lv_style_init(&style_bar_indic);
  lv_style_set_bg_opa(&style_bar_indic, LV_OPA_COVER);
  lv_style_set_bg_color(&style_bar_indic, lv_color_hex(0x444444));
  bar = lv_bar_create(lv_screen_active());
  lv_obj_remove_style_all(bar);
  lv_obj_add_style(bar, &style_bar, LV_PART_MAIN);
  lv_obj_add_style(bar, &style_bar_indic, LV_PART_INDICATOR);
  lv_obj_set_size(bar, 256, 32);
  lv_obj_set_pos(bar, 0, 0);

  int32_t value = 0;
  while (1) {
    value++;
    lv_bar_set_value(bar, value, LV_ANIM_OFF);
    if (value >= 100) {
      value = 0;
    }

    lv_timer_periodic_handler();
    // lv_timer_handler();
    vTaskDelay(pdMS_TO_TICKS(1));
  }
}
