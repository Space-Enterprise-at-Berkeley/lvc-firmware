#include "lvb.h"
#include "mpeg_ts.h"
#include "driver/parlio_tx.h"
#include "driver/parlio_types.h"

uint32_t bin_to_gray(uint32_t in);

esp_err_t init_phy(parlio_tx_unit_handle_t *tx_unit) {
    parlio_tx_unit_config_t config = {
        .clk_src = PARLIO_CLK_SRC_DEFAULT,      // Select the default clock source
        .data_width = TX_WIDTH,                        // Data width is 4 bits
        .clk_in_gpio_num = -1,                  // Do not use an external clock source
        .valid_gpio_num = PIN_CS,       // Use the valid signal as chip select
        .clk_out_gpio_num = PIN_CLK,
        .data_gpio_nums = PINS_DATA,
        .output_clk_freq_hz = 10 * 1000 * 1000, // Output clock frequency is 10 MHz
        .trans_queue_depth = 196,                // Transaction queue depth is 32
        .max_transfer_size = 256,               // Maximum transfer size is 256 bytes
        .shift_edge = PARLIO_SHIFT_EDGE_NEG,    // Shift data on the falling edge of the clock
        .flags = {
            .invert_valid_out = true, // The valid signal is high by default, inverted to simulate the chip select signal CS in QPI timing
        }
    };

    // Create TX unit instance
    ESP_ERROR_CHECK(parlio_new_tx_unit(&config, tx_unit));
    // Enable TX unit
    ESP_ERROR_CHECK(parlio_tx_unit_enable(*tx_unit));

    return ESP_OK;
}

esp_err_t close_phy(parlio_tx_unit_handle_t tx_unit) {
    ESP_ERROR_CHECK(parlio_tx_unit_disable(tx_unit));
    return parlio_del_tx_unit(tx_unit);
}

esp_err_t transmit_packet(parlio_tx_unit_handle_t tx_unit, const void *packet) {
    // Configure TX unit transmission parameters
    parlio_transmit_config_t transmit_config = {
        .idle_value = 0x00, // All data lines are low in idle state
    };

    // Do some bullshit FEC and gray coding
    uint32_t *frames = (uint32_t *)packet;
    for (int i = 0; i < TS_PACKET_LENGTH / 4; i++) {
        frames[i] = bin_to_gray(frames[i]);
    }

    return parlio_tx_unit_transmit(tx_unit, packet, TS_PACKET_LENGTH * sizeof(uint8_t) * 8, &transmit_config);
}

esp_err_t wait_for_tx(parlio_tx_unit_handle_t tx_unit) {
    return parlio_tx_unit_wait_all_done(tx_unit, -1);
}

uint32_t bin_to_gray(uint32_t in) {
    in ^= in >> 16;
    in ^= in >> 8;
    in ^= in >> 4;
    in ^= in >> 2;
    in ^= in >> 1;
    return in;
}
