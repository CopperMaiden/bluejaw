#include "comms.h"

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_err.h"
#include "esp_log.h"

static const char *TAG = "comms";

// Internal state
static poll_state_t s_poll = {
    .current_module_index = 0,
    .module_ids = {MODULE_B2_ADC, MODULE_B3_DISPLAY, MODULE_B4_ENCODER},
    .rx_pins = {B2_RX_PIN, B3_RX_PIN, B4_RX_PIN},
    .tx_pins = {B2_TX_PIN, B3_TX_PIN, B4_TX_PIN}
};

// Most recent readings to be written by poll task (read by application layer)
static adc_data_t s_latest_adc = {0};
static encoder_data_t s_latest_encoder = {0};

// CRC-8 (Polynomial 0x07, init 0x00)
uint8_t comms_cr8(const uint8_t *data, size_t len){
    uint8_t crc = 0x00;
    for (size_t i = 0; i < len; i++){
        crc ^= data[i];
        for (int bit = 0; bit < 8; bit++){
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// UART Initialization
void comms_init(void){
    uart_config_t cfg = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_param_config(UART_NUM, &cfg));

    // Starts on child module B2
    ESP_ERROR_CHECK(uart_set_pin(
        UART_NUM,
        s_poll.tx_pins[0],
        s_poll.rx_pins[0],
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    ESP_ERROR_CHECK(uart_driver_install(
        UART_NUM,
        1024,
        1024,
        0,
        NULL,
        0
    ));

    ESP_LOGI(TAG, "UART Initialized, starting on B2 (ADC)");

    // Task set to priority 5, prevent unresponsive behavior with other tasks alongside it (i2s, BLE)
    // Round robin poll task
    xTaskCreate(comms_poll_task, "comms_pull", 4096, NULL, 5, NULL);
}

// Pin switching (For UART, since there's only one port available)
void comms_switch_to_module(uint8_t module_index){
    // Flush any remaining data before pin reassignment
    uart_flush(UART_NUM);

    ESP_ERROR_CHECK(uart_set_pin(
        UART_NUM,
        s_poll.tx_pins[module_index],
        s_poll.rx_pins[module_index],
        UART_PIN_NO_CHANGE,
        UART_PIN_NO_CHANGE
    ));

    ESP_LOGD(TAG, "Switched to module 0x%02X (TX=%d, RX=%d)",
        s_poll.module_ids[module_index],
        s_poll.tx_pins[module_index],
        s_poll.rx_pins[module_index]
    );
}

// Turn packet struct into 8-byte buffer for transmission
static void packet_to_bytes(const comms_packet_t *p, uint8_t out[PACKET_LEN]){
    out[0] = p->start;
    out[1] = p->module_id;
    out[2] = p->cmd;
    memcpy(&out[3], p->payload, PACKET_PAYLOAD_LEN);
    out[7] = p->crc8;
}

// Turn 8-byte buffer into packet struct for recieving
static void bytes_to_packet(const uint8_t in[PACKET_LEN], comms_packet_t *out){
    out->start = in[0];
    out->module_id = in[1];
    out->cmd = in[2];
    memcpy(out->payload, &in[3], PACKET_PAYLOAD_LEN);
    out->crc8 = in[7];
}

// Compute CRC for all bytes except the 8th
static uint8_t packet_crc(const comms_packet_t *p){
    uint8_t buf[PACKET_LEN - 1];
    buf[0] = p->start;
    buf[1] = p->module_id;
    buf[2] = p->cmd;
    memcpy(&buf[3], p->payload, PACKET_LEN);
    return comms_cr8(buf, sizeof(buf));
}

void comms_build_poll(comms_packet_t *out, uint8_t module_id){
    memset(out, 0, sizeof(out));
    out->start = PACKET_START_BYTE;
    out->module_id = module_id;
    out->cmd = CMD_POLL;
    out->crc8 = packet_crc(out);
}

void comms_build_display_cmd(comms_packet_t *out, uint8_t sub_cmd, const uint8_t args[3]){
    memset(out, 0, sizeof(*out));
    out->start        = PACKET_START_BYTE;
    out->module_id    = MODULE_B3_DISPLAY;
    out->cmd          = CMD_DISPLAY_WRITE;
    out->payload[0]   = sub_cmd;
    out->payload[1]   = args[0];
    out->payload[2]   = args[1];
    out->payload[3]   = args[2];
    out->crc8         = packet_crc(out);
}

void comms_build_display_clear(comms_packet_t *out) {
    memset(out, 0, sizeof(*out));
    out->start = PACKET_START_BYTE;
    out->module_id = MODULE_B3_DISPLAY;
    out->cmd = CMD_DISPLAY_CLEAR;
    out-> crc8 = packet_crc(out);
}

// Send and recieve logic
esp_err_t comms_send_packet(const comms_packet_t *packet) {
    uint8_t buf[PACKET_LEN];
    packet_to_bytes(packet, buf);
    
    int written = uart_write_bytes(UART_NUM, (const char *)buf, PACKET_LEN);
    if (written != PACKET_LEN) {
        ESP_LOGE(TAG, "UART write failed: wrote %d of %d bytes", written, PACKET_LEN);
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t comms_recv_packet(comms_packet_t *out_packet) {
    uint8_t buf[PACKET_LEN];
    int timeout_ticks = pdMS_TO_TICKS(POLL_TIMEOUT_MS);

    int received = uart_read_bytes(UART_NUM, buf, PACKET_LEN, timeout_ticks);

    if(received != PACKET_LEN) {
        ESP_LOGW(TAG, "Timeout or short read: got %d bytes", received);
        return ESP_ERR_TIMEOUT;
    }

    // Validate start byte
    if(buf[0] != PACKET_START_BYTE) {
        ESP_LOGW(TAG, "Bad start byte: 0x%02X", buf[0]);
        uart_flush_input(UART_NUM);
        return ESP_ERR_INVALID_RESPONSE;
    }

    bytes_to_packet(buf, out_packet);

    // Validate CRC
    uint8_t expected_crc = packet_crc(out_packet);
    if (out_packet->crc8 != expected_crc) {
        ESP_LOGW(TAG, "CRC mismatch: got 0x%02X, expected 0x%02X", out_packet->crc8, expected_crc);
        return ESP_ERR_INVALID_CRC;
    }

    return ESP_OK;
}

// Response processing

esp_err_t comms_parse_response(const comms_packet_t *packet,
                                adc_data_t     *adc_out,
                                encoder_data_t *enc_out) {
    
    switch (packet->cmd) {

        case RSP_ADC_DATA:
            // payload[0..1] = big-endian uint16 ADC reading
            if (adc_out) {
                adc_out->raw_value = ((uint16_t)packet->payload[0] << 8)
                                    | (uint16_t)packet->payload[1];
            }
            ESP_LOGD(TAG, "ADC: %u", adc_out ? adc_out->raw_value : 0);
            break;

        case RSP_ENCODER_DATA:
            // payload[0] = signed delta, payload[1] button state
            if (enc_out) {
                enc_out->delta = (int8_t)packet->payload[0];
                enc_out->button_pressed = packet->payload[1];
            }
            ESP_LOGD(TAG, "Encoder delta=%d button %d",
                    enc_out ? enc_out->delta : 0,
                    enc_out ? enc_out->button_pressed :0);
            break;

        case RSP_DISPLAY_ACK:
            if (packet->payload[0] != 0x00) {
                ESP_LOGW(TAG, "Display returned error 0x%02X", packet->payload[0]);
            }
            break;

        case RSP_NACK:
            ESP_LOGW(TAG, "Module 0x%02X sent NACK", packet->module_id);
            return ESP_ERR_INVALID_RESPONSE;

        default:
            ESP_LOGW(TAG, "Unknown response cmd: 0x%02X", packet->cmd);
            return ESP_ERR_INVALID_RESPONSE;
    }

    return ESP_OK;
}

// Round robin polling

void comms_poll_task(void *param) {
    comms_packet_t tx_packet;
    comms_packet_t rx_packet;
    esp_err_t err;

    ESP_LOGI(TAG, "Poll task started - 10 ms per module, 30 ms full cycle");

    while(1) {
        uint8_t idx = s_poll.current_module_index;
        uint8_t module_id = s_poll.module_ids[idx];

        comms_switch_to_module(idx);

        comms_build_poll(&tx_packet, module_id);
        err = comms_send_packet(&tx_packet);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Send failed for module 0x%02X", module_id);
            goto next_module;
        }

        comms_parse_response(&rx_packet, &s_latest_adc, &s_latest_encoder);

        next_module:
            s_poll.current_module_index = (idx + 1) % MODULE_COUNT;

            vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}


// Accessors to read peripherals from main.c
adc_data_t comms_get_adc(void) {
    return s_latest_adc;
}

encoder_data_t comms_get_encoder(void) {
    return s_latest_encoder;
}