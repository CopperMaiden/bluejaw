#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/i2s_std.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "esp_log.h"
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "services/gap/ble_svc_gap.h"
#include "sdkconfig.h"

static int ble_gap_event(struct ble_gap_event *event, void *arg);
void serialCommsSetup(void);
void bleSetup(void);

void app_main() {

    // Start UART and i2s communication
    // TODO: Figure out how to loop UART RX and TX pin switching alongside other tasks
    serialCommsSetup();

    // Startup BLE with GAP service to search available sources
    bleSetup();
}

// B2 -> B3 -> B4
const int RX_PINS[3] = {0, 5, 6};
const int TX_PINS[3] = {1, 3, 4};

void serialCommsSetup(void){
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

    // Set to std mode in 16 bit slots with bclk, ws, and serial data pins (also LR audio)
    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100),
        .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
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

// Global event handler for BLE (CHANGE LATER FOR ACTUAL IMPLEMENATION, THIS IS FROM SIMS YT TUTORIAL)
static int ble_gap_event(struct ble_gap_event *event, void *arg)
{
    struct ble_hs_adv_fields fields;

    switch (event->type)
    {
    // NimBLE event discovery
    case BLE_GAP_EVENT_DISC:
        ESP_LOGI("GAP", "GAP EVENT DISCOVERY");
        ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data);
        if (fields.name_len > 0)
        {
            printf("Name: %.*s\n", fields.name_len, fields.name);
        }
        break;
    default:
        break;
    }
    return 0;
}

void bleSetup(void){
    // START OF BLUETOOTH LOW ENERGY (NimBLE) SETUP
    // Create tag for host identity and a var for address (auto assign later)
    char *TAG = "Bluejaw";
    uint8_t ble_addr_type;

    struct ble_gap_disc_params disc_params;

    disc_params.filter_duplicates = 1;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    // Subroutine for discovery mode of ble
    ble_gap_disc(ble_addr_type, 10000, &disc_params, ble_gap_event, NULL);
    
    // Required by BLE to store connection information
    nvs_flash_init();

    // Activate host controller interface
    esp_nimble_hci_init();
    nimble_port_init();

    // Configure device name characteristic, then init GAP service
    ble_svc_gap_device_name_set("Bluejaw");
    ble_svc_gap_init();

    // Assign address
    ble_hs_id_infer_auto(0, &ble_addr_type);
    // END OF BLE SETUP
}