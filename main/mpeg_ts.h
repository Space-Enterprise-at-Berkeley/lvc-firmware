#ifndef MPEG_TS_H
#define MPEG_TS_H

#include "driver/parlio_types.h"
#include "esp_err.h"
#include "esp_async_crc.h"

#define TS_PACKET_LENGTH (188)
#define PES_INTER (0)

#define TS_CRC_LEN (4)
#define TS_TABLE_HEADER_LEN (13)
#define TS_TABLE_SECTION_LEN (TS_PACKET_LENGTH - TS_TABLE_HEADER_LEN - TS_CRC_LEN)
#define TS_UDP_DATAGRAM (TS_PACKET_LENGTH * 7)

#define TS_PMT_PID (0x1000)
#define TS_VIDEO_PID (0x0100)

struct transport_stream_obj {
    uint8_t pes_cc;
    uint8_t pat_cc;
    uint8_t pmt_cc;
    async_crc_handle_t crc_hdl;
    parlio_tx_unit_handle_t tx_unit;
};

typedef struct transport_stream_obj transport_stream_t;

/// Initializes a new MPEG transport stream and opens a multicast port.
esp_err_t create_ts(transport_stream_t *ts);
esp_err_t close_ts(transport_stream_t ts);

esp_err_t send_pes_packets(transport_stream_t *ts, const void *dataptr, size_t len);
esp_err_t send_pat(transport_stream_t *ts);
esp_err_t send_pmt(transport_stream_t *ts);

#endif