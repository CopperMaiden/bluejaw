#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_hidh_nimble.h"
#include "driver/i2s_std.h"

// B2 -> B3 -> B4
const int RX_PINS[3] = {0, 5, 6};
const int TX_PINS[3] = {1, 3, 4};


// Init comms between child devices and loop through connections
void app_main() {
    // UART CONFIG (b1 to b3 connection)
    const uart_port_t uart_num = UART_NUM_1;

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };

    ESP_ERROR_CHECK(uart_param_config(uart_num, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(
        uart_num,
        GPIO_NUM_1,                      // TX pin (choose valid GPIO)
        GPIO_NUM_0,                      // RX pin (choose valid GPIO)
        UART_PIN_NO_CHANGE,      // RTS not used
        UART_PIN_NO_CHANGE       // CTS not used
    ));

    ESP_ERROR_CHECK(uart_driver_install(
        uart_num,
        1024,   // RX buffer
        1024,   // TX buffer
        0,
        NULL,
        0
    ));

    

    // END OF UART CONFIG

    
    // START OF i2s CONFIG
    // Transmit from B1
    i2s_chan_handle_t tx_handle;

    // Explicit configuration
    i2s_chan_config_t chan_cfg = {
        .id = I2S_NUM_0,
        .role = I2S_ROLE_MASTER,
        .dma_desc_num = 6,
        .dma_frame_num = 240
    };

    // Create channel
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL)); // No RX necessary

    // Set to std mode in 32 bit slots with bclk, ws, and serial data pins (also LR audio)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
            .mclk = I2S_GPIO_UNUSED,
            .bclk = GPIO_NUM_7,
            .ws = GPIO_NUM_8,
            .dout = GPIO_NUM_9,
            .din = I2S_GPIO_UNUSED,
        },
    };

    // Init + enable new channel
    ESP_ERROwR_CHECK(i2s_channel_init_std_mode(tx_handle, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_handle));
    // END OF i2s CONFIG


}