#ifndef LVB_H
#define LVB_H

#include "driver/parlio_types.h"
#include "esp_err.h"
#include <stdint.h>

#define FRAME_WIDTH 32
#define TX_WIDTH 8
#define PINS_DATA {25, 24, 2, 3, 4, 5, 6, 14}
#define PIN_CS (20)
#define PIN_CLK (19)

esp_err_t init_phy(parlio_tx_unit_handle_t *tx_unit);
esp_err_t close_phy(parlio_tx_unit_handle_t tx_unit);
esp_err_t transmit_packet(parlio_tx_unit_handle_t tx_unit, const void *packet);
esp_err_t wait_for_tx(parlio_tx_unit_handle_t tx_unit);

#endif