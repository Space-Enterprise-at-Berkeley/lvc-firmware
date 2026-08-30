#include "esp_log.h"
#include "esp_h264_enc_single_hw.h"
#include "esp_h264_alloc.h"
#include "esp_log_level.h"
#include "freertos/idf_additions.h"
#include "lvb.h"
#include "stream.h"
#include "mpeg_ts.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include <sys/_timeval.h>
#include <sys/param.h>
#include <sys/time.h>

#define PRO_CPU 0
#define APP_CPU 1

static const char *TAG = "lvc::main.c";

static int stream_fd;

static uint8_t *swap_buffer;
static uint32_t swap_data_length;

static TaskHandle_t xTxTask = NULL, xEncTask = NULL;

void transmit_task(void *pvParameter) {
    xTaskNotifyGiveIndexed(xEncTask, 0);

    transport_stream_t ts;
    esp_err_t err = create_ts(&ts);

    while (err < ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize transport stream");
        close_ts(ts);
        vTaskDelay(5 / portTICK_PERIOD_MS);
        err = create_ts(&ts);
    }

    int frame_count = 0;

    for (;;) {
        ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
        // Somehow we need to move all of this MPEG-TS logic on to the protocol core
        // That way we can split up the FEC encoding and IO writing packet-wise to lessen the surge load on LVB

        // Send PAT and PMT every 3 frames
        if (frame_count++ % 3 == 0) {
            send_pat(&ts);
            send_pmt(&ts);
        }

        ESP_LOGI(TAG, "Sending PES packets for frame of size %d", swap_data_length);
        if (send_pes_packets(&ts, swap_buffer, swap_data_length) != ESP_OK) {
            ESP_LOGW(TAG, "Failed to send PES packets for frame of size %d", swap_data_length);
        }

        xTaskNotifyGiveIndexed(xEncTask, 0);
    }
}

void encode_task(void *pvParameter) {
    esp_h264_enc_cfg_hw_t cfg = {0};
    cfg.gop = CONFIG_H264_GOP;
    cfg.fps = CONFIG_H264_FPS;
    cfg.res.width = WIDTH;
    cfg.res.height = HEIGHT;

    cfg.rc.bitrate = CONFIG_H264_BITRATE;
    cfg.rc.qp_min = CONFIG_H264_MIN_QP;
    cfg.rc.qp_max = CONFIG_H264_MAX_QP;

    cfg.pic_type = ESP_H264_RAW_FMT_O_UYY_E_VYY;

    // Initialize encoder
    esp_h264_enc_t *enc = NULL;
    int err = esp_h264_enc_hw_new(&cfg, &enc);
    if (err != 0) {
        ESP_LOGE(TAG, "Failed to initialize new h264 encoder");
        return;
    }

    // Allocate input/output buffers
    esp_h264_enc_in_frame_t in_frame = {.raw_data.len = BUFFER_LEN};
    in_frame.raw_data.buffer = esp_h264_aligned_calloc(16, 1, in_frame.raw_data.len, &in_frame.raw_data.len, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
                                                
    esp_h264_enc_out_frame_t out_frame = {.raw_data.len = in_frame.raw_data.len};
    out_frame.raw_data.buffer = esp_h264_aligned_calloc(16, 1, out_frame.raw_data.len, &out_frame.raw_data.len, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);

    // Allocate the swap buffer in the same way to allow it to be used in place of `out_frame.raw_data.buffer`
    swap_buffer = esp_h264_aligned_calloc(16, 1, out_frame.raw_data.len, &out_frame.raw_data.len, MALLOC_CAP_DMA | MALLOC_CAP_SPIRAM);
    swap_data_length = 0;

    // Start encoding
    esp_h264_enc_open(enc);

    // Encoding loop
    for (;;) {
        int err = 0;
        
        while (err >= 0) {
            while (err >= 0) {
                err = stream_capture_frame(stream_fd, in_frame.raw_data.buffer, &in_frame.raw_data.len);
                if (err == ESP_ERR_TIMEOUT) {
                    vTaskDelay(5 / portTICK_PERIOD_MS);
                    err = ESP_OK;
                    continue;
                } else if (err != ESP_OK) {
                    break;
                }

                if (esp_h264_enc_process(enc, &in_frame, &out_frame) != ESP_H264_ERR_OK) {
                    ESP_LOGE(TAG, "error while encoding frame");
                    continue;
                }

                ESP_LOGI(TAG, "Sending packet of %d bytes", out_frame.length);
                ulTaskNotifyTakeIndexed(0, pdTRUE, portMAX_DELAY);
                
                // Swap buffers
                uint8_t *tmp_buf = swap_buffer;
                swap_buffer = out_frame.raw_data.buffer;
                out_frame.raw_data.buffer = tmp_buf;
                swap_data_length = out_frame.length;

                xTaskNotifyGiveIndexed(xTxTask, 0);
            }
        }
        
        ESP_LOGE(TAG, "Error sending multicast packet, closing socket and restarting");
    }

    // Resource release
    esp_h264_enc_close(enc);
    esp_h264_enc_del(enc);
    esp_h264_free(in_frame.raw_data.buffer);
}

void app_main(void) {
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_err_t ret = stream_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "video camera init failed");
        return;
    }

    stream_fd = stream_open();
    if (stream_fd < 0) {
        ESP_LOGE(TAG, "Failed to open camera stream");
        return;
    }

    xTaskCreatePinnedToCore(&encode_task, "encode_task", 4096, NULL, 5, &xEncTask, APP_CPU);
    xTaskCreatePinnedToCore(&transmit_task, "transmit_task", 4096, NULL, 5, &xTxTask, PRO_CPU);
}
