/*
 * Copyright (c) Blecon Ltd
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include <zephyr/shell/shell.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led.h>
#include <zephyr/input/input.h>

#include "stdio.h"
#include "string.h"
#include "stdlib.h"

#include "blecon/blecon.h"
#include "blecon/blecon_error.h"
#include "blecon_zephyr/blecon_zephyr.h"
#include "blecon_zephyr/blecon_zephyr_event_loop.h"

#define FAST_BLINK_PERIOD_MS 200U
#define SLOW_BLINK_PERIOD_MS 500U

static struct blecon_event_loop_t* _event_loop = NULL;
static struct blecon_t _blecon = {0};
static struct blecon_request_t _request = {0};
static struct blecon_request_send_data_op_t _send_op = {0};
static struct blecon_request_receive_data_op_t _receive_op = {0};
static uint8_t _outgoing_data_buffer[64] = {0};
static uint8_t _incoming_data_buffer[64] = {0};

// Blecon callbacks
static void example_on_connection(struct blecon_t* blecon);
static void example_on_disconnection(struct blecon_t* blecon);
static void example_on_time_update(struct blecon_t* blecon);
static void example_on_ping_result(struct blecon_t* blecon);

const static struct blecon_callbacks_t blecon_callbacks = {
    .on_connection = example_on_connection,
    .on_disconnection = example_on_disconnection,
    .on_time_update = example_on_time_update,
    .on_ping_result = example_on_ping_result
};

// Requests callbacks
static void example_request_on_closed(struct blecon_request_t* request);
static void example_request_on_data_sent(struct blecon_request_send_data_op_t* send_data_op, bool data_sent);
static uint8_t* example_request_alloc_incoming_data_buffer(struct blecon_request_receive_data_op_t* receive_data_op, size_t sz);
static void example_request_on_data_received(struct blecon_request_receive_data_op_t* receive_data_op, bool data_received, const uint8_t* data, size_t sz, bool finished);

const static struct blecon_request_callbacks_t blecon_request_callbacks = {
    .on_closed = example_request_on_closed,
    .on_data_sent = example_request_on_data_sent,
    .alloc_incoming_data_buffer = example_request_alloc_incoming_data_buffer,
    .on_data_received = example_request_on_data_received
};

// Event handlers functions
static void cmd_blecon_connection_initiate_event(struct blecon_event_t* event, void* user_data);
static void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data);

// Event handlers
static struct blecon_event_t* _cmd_blecon_connection_initiate_event = NULL;
static struct blecon_event_t* _cmd_blecon_announce_event = NULL;

// Devices
const static struct device *led_pwm;

static void button_handler(struct input_event *evt, void* user_data) {
  switch (evt->code) {
    case INPUT_KEY_0:
        if(evt->value == 1) {
            blecon_event_signal(_cmd_blecon_announce_event);
        }
        break;
    default:
        break;
    }
}

INPUT_CALLBACK_DEFINE(NULL, button_handler, NULL);

static void led_timeout(struct k_timer *timer) {
    int ret;
    ret = led_off(led_pwm, 0);
    if(ret < 0) {
        printk("led off err=%d\r\n", ret);
        return;
    }
}
K_TIMER_DEFINE(led_timer, led_timeout, NULL);

void example_on_connection(struct blecon_t* blecon) {
    printk("Connected, sending request.\r\n");

    // Clean-up request
    blecon_request_cleanup(&_request);

    // Create message
    sprintf((char*)_outgoing_data_buffer, "Hello blecon!");

    // Create send data operation
    if(!blecon_request_send_data(&_send_op, &_request, _outgoing_data_buffer,
        strlen((char*)_outgoing_data_buffer), true, NULL)) {
        printk("Failed to send data\r\n");
        blecon_request_cleanup(&_request);
        return;
    }

    // Create receive data operation
    if(!blecon_request_receive_data(&_receive_op, &_request, NULL)) {
        printk("Failed to receive data\r\n");
        blecon_request_cleanup(&_request);
        return;
    }

    // Submit request
    blecon_submit_request(&_blecon, &_request);
}

void example_on_disconnection(struct blecon_t* blecon) {
    printk("Disconnected\r\n");
}

void example_on_time_update(struct blecon_t* blecon) {
    printk("Time update\r\n");
}

void example_on_ping_result(struct blecon_t* blecon) {}

void example_request_on_data_received(struct blecon_request_receive_data_op_t* receive_data_op, bool data_received, const uint8_t* data, size_t sz, bool finished) {
    // Retrieve response
    if(!data_received) {
        printk("Failed to receive data\r\n");
        return;
    }
    printk("Data received\r\n");

    static char message[64] = {0};
    memset(message, 0, sizeof(message));
    memcpy(message, data, sz);

    printk("Frame: %s\r\n", message);

    if(finished) {
        printk("All received\r\n");
    }
}

void example_request_on_data_sent(struct blecon_request_send_data_op_t* send_data_op, bool data_sent) {
    if(!data_sent) {
        printk("Failed to send data\r\n");
        return;
    }
    printk("Data sent\r\n");
}

uint8_t* example_request_alloc_incoming_data_buffer(struct blecon_request_receive_data_op_t* receive_data_op, size_t sz) {
    return _incoming_data_buffer;
}

void example_request_on_closed(struct blecon_request_t* request) {
    enum blecon_request_status_code_t status_code = blecon_request_get_status(request);

    if(status_code != blecon_request_status_ok) {
        printk("Request failed with status code: %d\r\n", status_code);
    } else {
        printk("Request successful\r\n");
    }

    // Terminate connection
    blecon_connection_terminate(&_blecon);
}

int main(void)
{
    // Initialize Devices
    led_pwm = DEVICE_DT_GET(DT_PATH(pwmleds));
    if (!device_is_ready(led_pwm)) {
        printk("Device %s is not ready\r\n", led_pwm->name);
        return 0;
    }

    // Get event loop
    _event_loop = blecon_zephyr_get_event_loop();

    // Get modem
    struct blecon_modem_t* modem = blecon_zephyr_get_modem();

    // Register event ids for shell commands
    _cmd_blecon_connection_initiate_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_connection_initiate_event, NULL);
    _cmd_blecon_announce_event = blecon_event_loop_register_event(_event_loop, cmd_blecon_announce_event, NULL);

    // Blecon
    blecon_init(&_blecon, modem);
    blecon_set_callbacks(&_blecon, &blecon_callbacks, NULL);
    if(!blecon_setup(&_blecon)) {
        printk("Failed to setup blecon\r\n");
        return 1;
    }

    // Init request
    const static struct blecon_request_parameters_t request_params = {
        .namespace = "aura",
        .method = "hello",
        .oneway = false,
        .request_content_type = "text/plain",
        .response_content_type = "text/plain",
        .response_mtu = sizeof(_incoming_data_buffer),
        .callbacks = &blecon_request_callbacks,
        .user_data = NULL
    };
    blecon_request_init(&_request, &request_params);

    // Print device URL
    char blecon_url[BLECON_URL_SZ] = {0};
    if(!blecon_get_url(&_blecon, blecon_url, sizeof(blecon_url))) {
        printk("Failed to get device URL\r\n");
        return 1;
    }
    printk("Device URL: %s\r\n", blecon_url);

    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return 1;
    }

    // Enter main loop.
    blecon_event_loop_run(_event_loop);

    // Won't reach here
    return 0;
}

void cmd_blecon_connection_initiate_event(struct blecon_event_t* event, void* user_data) {
    // Initiate connection
    if(!blecon_connection_initiate(&_blecon)) {
        printk("Failed to initiate connection\r\n");
        return;
    }
}

void cmd_blecon_announce_event(struct blecon_event_t* event, void* user_data) {
    // Announce device ID
    int ret;

    ret = led_blink(led_pwm, 0, FAST_BLINK_PERIOD_MS/2U, FAST_BLINK_PERIOD_MS/2U);
    if(ret < 0) {
        printk("blink error=%d\r\n", ret);
        return;
    }

    if(!blecon_announce(&_blecon)) {
        printk("Error: %s\r\n", "announce");
        return;
    }

    k_timer_start(&led_timer, K_SECONDS(5), K_FOREVER);
}
