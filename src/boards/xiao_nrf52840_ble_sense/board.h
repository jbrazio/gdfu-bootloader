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

#ifndef _XIAO_BLE_SENSE
#define _XIAO_BLE_SENSE

#define _PINNUM(port, pin)    ((port)*32 + (pin))

/*------------------------------------------------------------------*/
/* LED
 *------------------------------------------------------------------*/
// The board has 3 leds, but changing the number here causes OTA issues.
#define LEDS_NUMBER           1
#define LED_PRIMARY_PIN       _PINNUM(0, 26)
#define LED_STATE_ON          0

#define NEOPIXELS_NUMBER      0

/*------------------------------------------------------------------*/
/* BUTTON
 *------------------------------------------------------------------*/
#define BUTTONS_NUMBER        2
#define BUTTON_1              _PINNUM(0, 18)
// Button 2 is defined as D1 from expansion board.
#define BUTTON_2              _PINNUM(0, 3)
#define BUTTON_PULL           NRF_GPIO_PIN_PULLUP

//--------------------------------------------------------------------+
// BLE OTA
//--------------------------------------------------------------------+
#define BLEDIS_MANUFACTURER   "Seeed"
#define BLEDIS_MODEL          "XIAO nRF52840 Sense"

//--------------------------------------------------------------------+
// USB
//--------------------------------------------------------------------+
#define USB_DESC_VID           0x2886
#define USB_DESC_UF2_PID       0x0045
#define USB_DESC_CDC_ONLY_PID  0x0045

//------------- UF2 -------------//
#define UF2_PRODUCT_NAME   "Seeed XIAO nRF52840"
#define UF2_VOLUME_LABEL   "XIAO-SENSE"
#define UF2_BOARD_ID       "nRF52840-SeeedXiaoSense-v1"
#define UF2_INDEX_URL      "https://www.seeedstudio.com/"

//--------------------------------------------------------------------+
// Radio (SX1262) GFSK DFU configuration (not hardware-validated)
// Seeed Wio-SX1262 module: RXEN only (no TXEN), DIO2 RF switch, DIO3 TCXO 1.8 V
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

// BUTTON_2 (P0.03) is the SX1262 DIO1 line; do not read it as a button in GFSK builds.
#if defined(GFSK_DFU_ENABLED) && (GFSK_DFU_ENABLED == 1)
#define BUTTON_FRESET         BUTTON_1
#endif

#endif // _XIAO_BLE_SENSE
