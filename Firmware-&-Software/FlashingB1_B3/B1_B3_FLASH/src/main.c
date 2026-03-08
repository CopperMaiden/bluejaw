#include <string.h>
#include "esp_log.h"
#include "esp32_port.h"
#include "esp_loader.h"
#include "driver/gpio.h"
#include "driver/uart.h"

static const char *TAG = "serial_flasher";

extern const uint8_t bootloader_bin_start[]      asm("_binary_bootloader_bin_start");
extern const uint8_t bootloader_bin_end[]        asm("_binary_bootloader_bin_end");
extern const uint8_t partition_table_bin_start[] asm("_binary_partition_table_bin_start");
extern const uint8_t partition_table_bin_end[]   asm("_binary_partition_table_bin_end");
extern const uint8_t app_bin_start[]             asm("_binary_app_bin_start");
extern const uint8_t app_bin_end[]               asm("_binary_app_bin_end");

static esp_loader_error_t flash_partition(const uint8_t *start, const uint8_t *end, uint32_t address)
{
    uint32_t size = end - start;
    uint32_t bytes_sent = 0;

    if (esp_loader_flash_start(address, size, 4096) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Flash start failed at 0x%x", address);
        return ESP_LOADER_ERROR_FAIL;
    }

    while (bytes_sent < size) {
        uint32_t chunk = MIN(4096, size - bytes_sent);
        if (esp_loader_flash_write((void *)(start + bytes_sent), chunk) != ESP_LOADER_SUCCESS) {
            ESP_LOGE(TAG, "Flash write failed at 0x%x", address);
            return ESP_LOADER_ERROR_FAIL;
        }
        bytes_sent += chunk;
    }

    return ESP_LOADER_SUCCESS;
}

void app_main(void)
{
    const loader_esp32_config_t config = {
        .baud_rate         = 115200,
        .uart_port         = UART_NUM_1,
        .uart_tx_pin       = GPIO_NUM_1,
        .uart_rx_pin       = GPIO_NUM_0,
        .reset_trigger_pin = -1,
        .gpio0_trigger_pin = -1,
    };

    if (loader_port_esp32_init(&config) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Port init failed");
        return;
    }

    esp_loader_connect_args_t connect_args = ESP_LOADER_CONNECT_DEFAULT();
    if (esp_loader_connect(&connect_args) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Failed to connect to slave");
        return;
    }

    ESP_LOGI(TAG, "Flashing bootloader...");
    if (flash_partition(bootloader_bin_start, bootloader_bin_end, 0x1000) != ESP_LOADER_SUCCESS) {
        return;
    }

    ESP_LOGI(TAG, "Flashing partition table...");
    if (flash_partition(partition_table_bin_start, partition_table_bin_end, 0x8000) != ESP_LOADER_SUCCESS) {
        return;
    }

    ESP_LOGI(TAG, "Flashing app...");
    if (flash_partition(app_bin_start, app_bin_end, 0x10000) != ESP_LOADER_SUCCESS) {
        return;
    }

    if (esp_loader_flash_finish(true) != ESP_LOADER_SUCCESS) {
        ESP_LOGE(TAG, "Flash finish failed");
        return;
    }

    ESP_LOGI(TAG, "Done!");
}