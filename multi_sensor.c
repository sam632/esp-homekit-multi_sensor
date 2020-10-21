#include <stdio.h>
#include <stdlib.h>
#include <espressif/esp_common.h>
#include <esp/uart.h>
#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>
#include <string.h>

//For LCD Screen
#include <i2c/i2c.h>
#include <hd44780/hd44780.h>

//For Homekit
#include <homekit/homekit.h>
#include <homekit/characteristics.h>

//For Wi-Fi
#include <wifi_config.h>

//For Sensors
#include <dht/dht.h>
#include <button.h>

//LCD pins
#define I2C_BUS 0
#define SCL_PIN 5  //D1
#define SDA_PIN 4  //D2
#define ADDR 0x27

//Define GPIOs
const int led_gpio = 14; //D5, the GPIO pin connected to the mood light

const int dht_gpio = 2; //D4, the GPIO pin connected to the DHT

const int motion_gpio = 12; //D6, the GPIO pin connected to the HC-SR501 motion sensor

const int button_gpio = 13; //D7, the GPIO pin connected to the button

bool password_displayed = false;

void identify_temp(homekit_value_t _value);
void identify_mot(homekit_value_t _value);
void identify_mood(homekit_value_t _value);
void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context);
void button_callback(button_event_t event, void* context);
TaskHandle_t UpdateTempTask;

//Degree symbol
static const uint8_t char_data[] = {
    0x0E, 0x0A, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00
};

//Define LCD
static const hd44780_t lcd = {
    .i2c_dev.bus = I2C_BUS,
    .i2c_dev.addr = ADDR,
    .font = HD44780_FONT_5X8,
    .lines = 2,
    .pins = {
        .rs = 0,
        .e  = 2,
        .d4 = 4,
        .d5 = 5,
        .d6 = 6,
        .d7 = 7,
        .bl = 3
    },
    .backlight = true
};

void led_write(bool on) {
    gpio_write(led_gpio, on ? 1 : 0);
}

void led_init() {
    gpio_enable(led_gpio, GPIO_OUTPUT);
    led_write(false);
}

void lcd_init() {
    i2c_init(I2C_BUS, SCL_PIN, SDA_PIN, I2C_FREQ_100K);
    
    hd44780_init(&lcd);
    hd44780_upload_character(&lcd, 0, char_data);
}

void display_init() {
    hd44780_gotoxy(&lcd, 0, 0);
    hd44780_puts(&lcd, "Temp:");
    
    hd44780_gotoxy(&lcd, 12, 0);
    hd44780_puts(&lcd, "\x08");
    hd44780_gotoxy(&lcd, 13, 0);
    hd44780_puts(&lcd, "C");
    
    hd44780_gotoxy(&lcd, 0, 1);
    hd44780_puts(&lcd, "Humidity:");
    
    hd44780_gotoxy(&lcd, 12, 1);
    hd44780_puts(&lcd, "%");
}

void identify_temp_task(void *_args) {
    // We identify the temperature sensor by writing on the display
    vTaskSuspend(UpdateTempTask);
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 4, 0);
    hd44780_puts(&lcd, "Identify");
    hd44780_gotoxy(&lcd, 2, 1);
    hd44780_puts(&lcd, "Temp Sensor");
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    hd44780_clear(&lcd);
    display_init();
    vTaskResume(UpdateTempTask);
    vTaskDelete(NULL);
}

void identify_temp(homekit_value_t _value) {
    printf("Temp Sensor Identify\n");
    xTaskCreate(identify_temp_task, "Temp Sensor Identify", 128, NULL, 2, NULL);
}

void identify_mot_task(void *_args) {
    // We identify the motion sensor by writing on the display
    vTaskSuspend(UpdateTempTask);
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 4, 0);
    hd44780_puts(&lcd, "Identify");
    hd44780_gotoxy(&lcd, 1, 1);
    hd44780_puts(&lcd, "Motion Sensor");
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    hd44780_clear(&lcd);
    display_init();
    vTaskResume(UpdateTempTask);
    vTaskDelete(NULL);
}

void identify_mot(homekit_value_t _value) {
    printf("Motion Sensor Identify\n");
    xTaskCreate(identify_mot_task, "Motion Sensor Identify", 128, NULL, 2, NULL);
}

void identify_mood_task(void *_args) {
    // We identify the mood light by writing on the display
    vTaskSuspend(UpdateTempTask);
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 4, 0);
    hd44780_puts(&lcd, "Identify");
    hd44780_gotoxy(&lcd, 3, 1);
    hd44780_puts(&lcd, "Mood Light");
    
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    
    hd44780_clear(&lcd);
    display_init();
    vTaskResume(UpdateTempTask);
    vTaskDelete(NULL);
}

void identify_mood(homekit_value_t _value) {
    printf("Mood Light Identify\n");
    xTaskCreate(identify_mood_task, "Mood Light Identify", 128, NULL, 2, NULL);
}

void reset_configuration_task() {
    
    vTaskSuspend(UpdateTempTask);
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 3, 0);
    hd44780_puts(&lcd, "Resetting");
    hd44780_gotoxy(&lcd, 2, 1);
    hd44780_puts(&lcd, "Multi Sensor");
    
    printf("Resetting Wifi Config\n");
    wifi_config_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Resetting HomeKit Config\n");
    homekit_server_reset();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    printf("Restarting\n");
    sdk_system_restart();
    vTaskResume(UpdateTempTask);
    vTaskDelete(NULL);
}

void reset_configuration() {
    printf("Resetting Sensor configuration\n");
    xTaskCreate(reset_configuration_task, "Reset configuration", 256, NULL, 2, NULL);
}

//Define password functions
void display_password() {
    vTaskSuspend(UpdateTempTask);
    
    hd44780_clear(&lcd);
    hd44780_gotoxy(&lcd, 2, 0);
    hd44780_puts(&lcd, "Set-Up Code:");
    hd44780_gotoxy(&lcd, 3, 1);
    hd44780_puts(&lcd, "137-22-976");
    printf("Password is 137-22-976\n");

    password_displayed = true;
}

void hide_password() {
    if (!password_displayed)
        return;

    hd44780_clear(&lcd);

    display_init();
    vTaskResume(UpdateTempTask);
    
    password_displayed = false;
}


//Define Homekit Characteristics
homekit_characteristic_t temperature = HOMEKIT_CHARACTERISTIC_(CURRENT_TEMPERATURE, 0);
homekit_characteristic_t humidity    = HOMEKIT_CHARACTERISTIC_(CURRENT_RELATIVE_HUMIDITY, 0);
homekit_characteristic_t motion_detected  = HOMEKIT_CHARACTERISTIC_(MOTION_DETECTED, 0);
homekit_characteristic_t switch_on = HOMEKIT_CHARACTERISTIC_(
    ON, false, .callback=HOMEKIT_CHARACTERISTIC_CALLBACK(switch_on_callback)
);

//Define callback functions
void switch_on_callback(homekit_characteristic_t *_ch, homekit_value_t on, void *context) {
    led_write(switch_on.value.bool_value);
}

void button_callback(button_event_t event, void* context) {
    switch (event) {
        case button_event_single_press:
            printf("Toggling LED due to single press\n");
            switch_on.value.bool_value = !switch_on.value.bool_value;
            led_write(switch_on.value.bool_value);
            homekit_characteristic_notify(&switch_on, switch_on.value);
            break;
        case button_event_double_press:
            printf("Toggling screen due to double press\n");
            //switch_on.value.bool_value = !switch_on.value.bool_value;
            //hd44780_set_backlight(&lcd, switch_on.value.bool_value);
            break;
        case button_event_long_press:
            reset_configuration();
            break;
        default:
            printf("Unknown button event\n");
    }
}

void button_init() {
    button_config_t button_config = BUTTON_CONFIG(
        button_active_low,
        .long_press_time = 3000,
        .max_repeat_presses = 2,
    );

    if (button_create(button_gpio, button_config, button_callback, NULL)) {
        printf("Failed to initialize button\n");
    }
}

void temperature_sensor_callback(void *_args) {
    gpio_set_pullup(dht_gpio, false, false);
    
    float humidity_value, temperature_value;
    
    char temp[4], hum[4];
    
    while (1) {
        bool success = dht_read_float_data(
            DHT_TYPE_DHT11, dht_gpio,
            &humidity_value, &temperature_value
        );
        if (success) {
        
            hd44780_gotoxy(&lcd, 10, 0);
            snprintf(temp, 3, "%f\n", temperature_value);
            hd44780_puts(&lcd, temp);

            hd44780_gotoxy(&lcd, 10, 1);
            snprintf(hum, 3, "%f\n", humidity_value);
            hd44780_puts(&lcd, hum);
            
            temperature.value.float_value = temperature_value;
            humidity.value.float_value = humidity_value;

            homekit_characteristic_notify(&temperature, HOMEKIT_FLOAT(temperature_value));
            homekit_characteristic_notify(&humidity, HOMEKIT_FLOAT(humidity_value));
            
        } else {
            printf("Could not read data from sensor\n");
        }

        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void temperature_sensor_init() {
    xTaskCreate(temperature_sensor_callback, "Temperatore Sensor", 512, NULL, 2, &UpdateTempTask);
}

void motion_sensor_callback(uint8_t gpio) {

    if (gpio == motion_gpio){
        int new = 0;
        new = gpio_read(motion_gpio);
        motion_detected.value = HOMEKIT_BOOL(new);
        homekit_characteristic_notify(&motion_detected, HOMEKIT_BOOL(new));
        if (new == 1) {
        printf("Motion Detected on %d\n", gpio);
        } else {
        printf("Motion Stopped on %d\n", gpio);

    }
    }
    else {
        printf("Interrupt on %d", gpio);
    }

}

void motion_init() {
    gpio_enable(motion_gpio, GPIO_INPUT);
    gpio_set_pullup(motion_gpio, false, false);
    gpio_set_interrupt(motion_gpio, GPIO_INTTYPE_EDGE_ANY, motion_sensor_callback);
}

homekit_accessory_t *accessories[] = {
    HOMEKIT_ACCESSORY(.id=1, .category=homekit_accessory_category_thermostat, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Multi Sensor"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Sam"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "012345"),
                HOMEKIT_CHARACTERISTIC(MODEL, "Multi Sensor"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_temp),
                NULL
        }),
        HOMEKIT_SERVICE(HUMIDITY_SENSOR, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Humidity Sensor"),
            &humidity,
            NULL
        }),
        HOMEKIT_SERVICE(TEMPERATURE_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]) {
            HOMEKIT_CHARACTERISTIC(NAME, "Temperature Sensor"),
            &temperature,
            NULL
        }),
        NULL
    }),
    
    HOMEKIT_ACCESSORY(.id=2, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]) {
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]) {
                HOMEKIT_CHARACTERISTIC(NAME, "Motion Sensor"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Sam"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "012346"),
                HOMEKIT_CHARACTERISTIC(MODEL, "Occupancy Sensor"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_mot),
                NULL
        }),
        HOMEKIT_SERVICE(MOTION_SENSOR, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Motion Sensor"),
            &motion_detected,
            NULL
        }),
        NULL
    }),
    HOMEKIT_ACCESSORY(.id=3, .category=homekit_accessory_category_switch, .services=(homekit_service_t*[]){
            HOMEKIT_SERVICE(ACCESSORY_INFORMATION, .characteristics=(homekit_characteristic_t*[]){
                HOMEKIT_CHARACTERISTIC(NAME, "Mood Light"),
                HOMEKIT_CHARACTERISTIC(MANUFACTURER, "Sam"),
                HOMEKIT_CHARACTERISTIC(SERIAL_NUMBER, "012347"),
                HOMEKIT_CHARACTERISTIC(MODEL, "Mood Light"),
                HOMEKIT_CHARACTERISTIC(FIRMWARE_REVISION, "1.0.0"),
                HOMEKIT_CHARACTERISTIC(IDENTIFY, identify_mood),
                NULL
        }),
        HOMEKIT_SERVICE(SWITCH, .primary=true, .characteristics=(homekit_characteristic_t*[]){
            HOMEKIT_CHARACTERISTIC(NAME, "Mood Light"),
            &switch_on,
            NULL
        }),
        NULL
    }),
    NULL
};

void on_password() {
    display_password();
}


void on_homekit_event(homekit_event_t event) {
    hide_password();
}


homekit_server_config_t config = {
    .accessories = accessories,
    .password = "137-22-976",
    .setupId="2SA8",
    .password_callback = on_password,
    .on_event = on_homekit_event,
};

void on_wifi_ready() {
    homekit_server_init(&config);
}

void user_init(void) {
    uart_set_baud(0, 115200);

    wifi_config_init("Multi_Sensor", NULL, on_wifi_ready);
    
    led_init();
    lcd_init();
    display_init();
    button_init();
    motion_init();
    temperature_sensor_init();
    
}


