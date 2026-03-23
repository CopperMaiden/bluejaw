#pragma once

#include <stdint.h>
#include "driver/uart.h"
#include "driver/gpio.h"

// Define macros for easy adjustments later on (Just reduces repetition)
#define MODULE_B2_ADC       0x02
#define MODULE_B3_DISPLAY   0x03
#define MODULE_B4_ENCODER   0x04

#define MODULE_COUNT        3

// Only using one UART port since the C3 only has 2, and the first one is linked to the programmer device
#define UART_NUM    UART_NUM_1


// Defining hardware specific GPIO to avoid innevitable confusion later on
#define B2_TX_PIN   GPIO_NUM_1
#define B2_RX_PIN   GPIO_NUM_0
#define B3_TX_PIN   GPIO_NUM_3
#define B3_RX_PIN   GPIO_NUM_5
#define B4_TX_PIN   GPIO_NUM_4
#define B4_RX_PIN   GPIO_NUM_6

// Parent to child modules packet structure
#define PACKET_START_BYTE   0xAA
#define PACKET_LEN             8
#define PACKET_PAYLOAD_LEN        4

// Parent to child commands
#define CMD_POLL 0x01           // Poll child device for data
#define CMD_DISPLAY_WRITE 0x02  // Draw on display
#define CMD_DISPLAY_CLEAR 0x03  // Clear display
#define CMD_ACK 0x04            // Acknowledge recieved data

// Child to parent commands
#define RSP_ADC_DATA        0x10 // ADC value from B2
#define RSP_ENCODER_DATA    0x11 // Encoder from B4
#define RSP_DISPLAY_ACK     0x12 // 0x00 if ok and 0x01 for error
#define RSP_NACK            0xFF // General error

// Display control macros
#define DISP_CMD_PRINT      0x01 // Print char at coordinates (char, x, y)
#define DISP_CMD_DRAW_RECT  0x02 // Draws a rectangle (x, y, size)
#define DISP_CMD_SET_CURSOR 0x03 // Move the cursor to coordinates (x, y)


// Packet struct
typedef struct {
    uint8_t start;
    uint8_t module_id;
    uint8_t cmd;
    uint8_t payload[PACKET_PAYLOAD_LEN];
    uint8_t crc8;
} comms_packet_t;


// Processed data from peripherals

typedef struct {
    uint16_t raw_value;
} adc_data_t;

typedef struct {
    int8_t delta;
    uint8_t button_pressed;
} encoder_data_t;

// "Round-robin" style polling

#define POLL_INTERVAL_MS    10
#define POLL_TIMEOUT_MS     5

typedef struct {
    uint8_t current_module_index;
    uint8_t module_ids[MODULE_COUNT];
    gpio_num_t tx_pins[MODULE_COUNT];
    gpio_num_t rx_pins[MODULE_COUNT];
}   poll_state_t;

// Public API

/**
* @brief    Initialize the UART port and polling
*           (Call once from app_main)
*/
void comms_init(void);

/**
 * @brief    FreeRTOS task entry point:
 *           DO NOT CALL DIRECTLY, USE xTaskCreate.
 *           (Call once from app_main.)
 *
 * @param    param Unused (pass NULL)
 */
void comms_poll_task(void *param);


/**
 * @brief   Send a pre-built packet to a module.
 * 
 * @param packet Pointer to a fully populated comms_packet_t.
 * @return ESP_OK on sucess, ESP_FAIL on UART error. 
 */
esp_err_t comms_send_packet(const comms_packet_t *packet);


/**
 * @brief   Block until a response packet arrives or timeout occurs
 * 
 * @param out_packet Populated on success.
 * @return ESP_OK if a valid packet was received, ESP_ERR_TIMEOUT otherwise.
 */
esp_err_t comms_recv_packet(comms_packet_t *out_packet);


/**
 * @brief Build a CMD_DISPLAY_CLEAR packet.
 */
void comms_build_display_clear(comms_packet_t *out);


/**
 * @brief Parse a received response packet into typed data from peripherals.
 *        Writes into whichever of adc_out / enc_out is relevant; others untouched.
 * 
 * @param packet Received packet to parse.
 * @param adc_out Output for ADC data (may be NULL).
 * @param enc_out Output for encoder data (may be NULL).
 * @return ESP_OK on recognized response, ESP_ERR_INVALID_RESPONSE on unknown cmd.
 */
esp_err_t comms_parse_response(const comms_packet_t *packet,
                                adc_data_t      *adc_out,
                                encoder_data_t  *enc_out);


/**
 * @brief Switch UART TX_RX pins to route to a specific module.
 *        Called by poll task before each transmission.
 * 
 * @param module_index Index into poll_state_t.module_ids (0, 1, or 2).
 */
void comms_switch_to_module(uint8_t module_index);

/**
 * @brief Compute CRC-8 (poly 0x07) over a byte buffer
 */
uint8_t comms_crc8(const uint8_t *data, size_t len);