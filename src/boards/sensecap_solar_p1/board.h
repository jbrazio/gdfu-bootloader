/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2018 Ha Thach for Adafruit Industries
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef _SEEED_SOLAR_NODE_P1_H
#define _SEEED_SOLAR_NODE_P1_H

#define _PINNUM(port, pin)    ((port)*32 + (pin))

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
#define LEDS_NUMBER           1
#define LED_PRIMARY_PIN       _PINNUM(0, 19)
#define LED_STATE_ON          1

#define NEOPIXELS_NUMBER      0

/*------------------------------------------------------------------*/
/* BUTTON
 *------------------------------------------------------------------*/
#define BUTTONS_NUMBER        2
#define BUTTON_1              _PINNUM(1, 1) // Hold user button while pressing reset to enter OTA DFU mode
#define BUTTON_2              _PINNUM(1, 1)
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Seeed"
#define BLEDIS_MODEL          "SenseCAP Solar Node P1"

//--------------------------------------------------------------------+
// USB
//--------------------------------------------------------------------+
#define USB_DESC_VID           0x2886
#define USB_DESC_UF2_PID       0x0044
#define USB_DESC_CDC_ONLY_PID  0x0044

//------------- UF2 -------------//
#define UF2_PRODUCT_NAME   "Seeed Solar Node P1"
#define UF2_VOLUME_LABEL   "SENSECAP"
#define UF2_BOARD_ID       "nRF52840-SeeedSenseCAPSolarP1-v1"
#define UF2_INDEX_URL      "https://www.seeedstudio.com/"

//--------------------------------------------------------------------+
// Radio (SX1262) GFSK DFU configuration (not hardware-validated)
// Same SX1262 wiring as XIAO: RXEN only (no TXEN), DIO2 RF switch, DIO3 TCXO 1.8 V
//--------------------------------------------------------------------+
#define SX1262_SPI_INSTANCE   2
#define SX1262_CS_PIN         _PINNUM(0, 4)
#define SX1262_RESET_PIN      _PINNUM(0, 28)
#define SX1262_BUSY_PIN       _PINNUM(0, 29)
#define SX1262_DIO1_PIN       _PINNUM(0, 3)
#define SX1262_SCK_PIN        _PINNUM(1, 13)
#define SX1262_MOSI_PIN       _PINNUM(1, 15)
#define SX1262_MISO_PIN       _PINNUM(1, 14)
#define SX1262_RXEN_PIN       _PINNUM(0, 5)

#endif // _SEEED_SOLAR_NODE_P1_H
