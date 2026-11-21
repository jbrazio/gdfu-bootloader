#include "gdfu_radio_hal.h"
#include "board.h"

#include <string.h>

#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrfx_spim.h"

#define SX1262_SET_STANDBY              0x80
#define SX1262_SET_TX                   0x83
#define SX1262_SET_RX                   0x82
#define SX1262_SET_RF_FREQUENCY         0x86
#define SX1262_CALIBRATE                0x89
#define SX1262_SET_PACKET_TYPE          0x8A
#define SX1262_SET_TX_PARAMS            0x8E
#define SX1262_SET_MODULATION_PARAMS    0x8B
#define SX1262_SET_PACKET_PARAMS        0x8C
#define SX1262_SET_BUFFER_BASE_ADDRESS  0x8F
#define SX1262_SET_DIO_IRQ_PARAMS       0x08
#define SX1262_GET_IRQ_STATUS           0x12
#define SX1262_CLEAR_IRQ_STATUS         0x02
#define SX1262_SET_DIO2_AS_RF_SWITCH    0x9D
#define SX1262_SET_DIO3_AS_TCXO         0x97
#define SX1262_SET_PA_CONFIG            0x95
#define SX1262_WRITE_BUFFER             0x0E
#define SX1262_READ_BUFFER              0x1E
#define SX1262_WRITE_REGISTER           0x0D
#define SX1262_READ_REGISTER            0x1D
#define SX1262_GET_STATUS               0xC0
#define SX1262_GET_RX_BUFFER_STATUS     0x13

#define SX1262_IRQ_TX_DONE              0x0001
#define SX1262_IRQ_RX_DONE              0x0002
#define SX1262_IRQ_TIMEOUT              0x0200
#define SX1262_IRQ_CRC_ERR              0x0040

static nrfx_spim_t s_spi = NRFX_SPIM_INSTANCE(SX1262_SPI_INSTANCE);
static volatile bool s_spi_done = false;
static bool s_spi_inited = false;
static uint8_t s_spi_rx_buf[258];

static void spim_event_handler(nrfx_spim_evt_t const *p_event, void *p_context)
{
  (void)p_context;
  if (p_event->type == NRFX_SPIM_EVENT_DONE)
  {
    s_spi_done = true;
  }
}

static int wait_busy(uint32_t timeout_us)
{
  while (nrf_gpio_pin_read(SX1262_BUSY_PIN))
  {
    if (timeout_us == 0)
    {
      return -1;
    }
    nrf_delay_us(1);
    timeout_us--;
  }
  return 0;
}

static bool wait_spi_done(void)
{
  uint32_t timeout_us = 10000;
  while (!s_spi_done)
  {
    if (timeout_us == 0)
    {
      return false;
    }
    nrf_delay_us(1);
    timeout_us--;
  }
  return true;
}

static int cmd_write(const uint8_t *cmd, size_t len)
{
  if (wait_busy(10000) != 0)
  {
    return -1;
  }
  nrf_gpio_pin_clear(SX1262_CS_PIN);
  s_spi_done = false;
  nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(cmd, len);
  if (nrfx_spim_xfer(&s_spi, &xfer, 0) != NRFX_SUCCESS || !wait_spi_done())
  {
    nrf_gpio_pin_set(SX1262_CS_PIN);
    return -1;
  }
  nrf_gpio_pin_set(SX1262_CS_PIN);
  return 0;
}

static int cmd_read(const uint8_t *wbuf, uint16_t wlen, uint8_t *rbuf, uint16_t rlen)
{
  if (wait_busy(10000) != 0)
  {
    return -1;
  }
  nrf_gpio_pin_clear(SX1262_CS_PIN);

  s_spi_done = false;
  nrfx_spim_xfer_desc_t xfer = NRFX_SPIM_XFER_TX(wbuf, wlen);
  if (nrfx_spim_xfer(&s_spi, &xfer, 0) != NRFX_SUCCESS || !wait_spi_done())
  {
    nrf_gpio_pin_set(SX1262_CS_PIN);
    return -1;
  }

  if (wait_busy(10000) != 0)
  {
    nrf_gpio_pin_set(SX1262_CS_PIN);
    return -1;
  }

  static uint8_t dummy_tx[4] = {0};
  s_spi_done = false;
  nrfx_spim_xfer_desc_t rxfer = NRFX_SPIM_SINGLE_XFER(dummy_tx, 1, rbuf, rlen);
  if (nrfx_spim_xfer(&s_spi, &rxfer, 0) != NRFX_SUCCESS || !wait_spi_done())
  {
    nrf_gpio_pin_set(SX1262_CS_PIN);
    return -1;
  }

  nrf_gpio_pin_set(SX1262_CS_PIN);
  return 0;
}

/* Optional front-end pins; boards without them (T114) compile to nothing. */
static void front_end_rx(bool on)
{
#ifdef SX1262_RXEN_PIN
  nrf_gpio_pin_write(SX1262_RXEN_PIN, on);
#else
  (void) on;
#endif
}

int gdfu_radio_init(void)
{
  if (!s_spi_inited)
  {
#ifdef SX1262_POWER_EN_PIN
    nrf_gpio_cfg_output(SX1262_POWER_EN_PIN);
    nrf_gpio_pin_set(SX1262_POWER_EN_PIN);
    nrf_delay_ms(10);
#endif
#ifdef SX1262_RXEN_PIN
    nrf_gpio_cfg_output(SX1262_RXEN_PIN);
    nrf_gpio_pin_clear(SX1262_RXEN_PIN);
#endif
    nrf_gpio_cfg_output(SX1262_CS_PIN);
    nrf_gpio_pin_set(SX1262_CS_PIN);
    nrf_gpio_cfg_output(SX1262_RESET_PIN);
    nrf_gpio_pin_set(SX1262_RESET_PIN);
    nrf_gpio_cfg_input(SX1262_BUSY_PIN, NRF_GPIO_PIN_NOPULL);
    nrf_gpio_cfg_input(SX1262_DIO1_PIN, NRF_GPIO_PIN_NOPULL);

    nrfx_spim_config_t cfg = {
      .sck_pin = SX1262_SCK_PIN,
      .mosi_pin = SX1262_MOSI_PIN,
      .miso_pin = SX1262_MISO_PIN,
      .ss_pin = NRFX_SPIM_PIN_NOT_USED,
      .irq_priority = 7,
      .orc = 0xFF,
      .frequency = NRF_SPIM_FREQ_8M,
      .mode = NRF_SPIM_MODE_0,
      .bit_order = NRF_SPIM_BIT_ORDER_MSB_FIRST,
    };
    if (nrfx_spim_init(&s_spi, &cfg, spim_event_handler, NULL) != NRFX_SUCCESS)
    {
      return -1;
    }
    s_spi_inited = true;
  }

  nrf_gpio_pin_clear(SX1262_RESET_PIN);
  nrf_delay_ms(2);
  nrf_gpio_pin_set(SX1262_RESET_PIN);
  nrf_delay_ms(15);

  uint8_t standby[2] = { SX1262_SET_STANDBY, 0x00 };
  if (cmd_write(standby, sizeof(standby)) != 0) return -1;

  uint8_t tcxo[5] = { SX1262_SET_DIO3_AS_TCXO, 0x02, 0x00, 0x01, 0x40 };
  if (cmd_write(tcxo, sizeof(tcxo)) != 0) return -1;

  uint8_t rfsw[2] = { SX1262_SET_DIO2_AS_RF_SWITCH, 0x01 };
  if (cmd_write(rfsw, sizeof(rfsw)) != 0) return -1;

  uint8_t cal[2] = { SX1262_CALIBRATE, 0x7F };
  cmd_write(cal, sizeof(cal));
  nrf_delay_ms(5);

  /* CalibrateImage per datasheet table 9-2 */
#if GDFU_FREQUENCY_HZ > 800000000u
  uint8_t cimg[3] = { 0x98, 0xD7, 0xD8 };  /* 863–870 MHz band */
#else
  uint8_t cimg[3] = { 0x98, 0xD7, 0xDB };  /* 430–440 MHz band */
#endif
  cmd_write(cimg, sizeof(cimg));
  nrf_delay_ms(5);

  uint8_t pkt_type[2] = { SX1262_SET_PACKET_TYPE, 0x00 };
  if (cmd_write(pkt_type, sizeof(pkt_type)) != 0) return -1;

  uint32_t rf_freq = (uint32_t)(((uint64_t)GDFU_FREQUENCY_HZ << 25) / 32000000ULL);
  uint8_t freq_cmd[5] = {
    SX1262_SET_RF_FREQUENCY,
    (uint8_t)(rf_freq >> 24),
    (uint8_t)(rf_freq >> 16),
    (uint8_t)(rf_freq >> 8),
    (uint8_t)rf_freq,
  };
  if (cmd_write(freq_cmd, sizeof(freq_cmd)) != 0) return -1;

  uint8_t pa[5] = { SX1262_SET_PA_CONFIG, 0x04, 0x07, 0x00, 0x01 };
  if (cmd_write(pa, sizeof(pa)) != 0) return -1;

  if (gdfu_radio_set_tx_power(GDFU_DEFAULT_TX_POWER_DBM) != 0) return -1;

  uint32_t br = (32u * 32000000u) / GDFU_BITRATE_BPS;
  uint32_t fdev = ((uint64_t)GDFU_DEVIATION_HZ << 25) / 32000000u;
  uint8_t mod[9] = {
    SX1262_SET_MODULATION_PARAMS,
    (uint8_t)(br >> 16), (uint8_t)(br >> 8), (uint8_t)br,
    0x09,
    0x0B,  /* rxBw = 187.2 kHz (both profiles) */
    (uint8_t)(fdev >> 16), (uint8_t)(fdev >> 8), (uint8_t)fdev,
  };
  if (cmd_write(mod, sizeof(mod)) != 0) return -1;

  uint8_t pkt_params[10] = {
    SX1262_SET_PACKET_PARAMS,
    0x00, 0x20,
    0x05,
    0x10,
    0x00,
    0x01,
    0xFF,
    0x02,
    0x01,
  };
  if (cmd_write(pkt_params, sizeof(pkt_params)) != 0) return -1;

  uint8_t sync[6] = { SX1262_WRITE_REGISTER, 0x06, 0xC0, 0x13, 0x37, 0x00 };
  if (cmd_write(sync, sizeof(sync)) != 0) return -1;

  /* TX modulation workaround (mandatory per Semtech errata for GFSK):
   * set bit 2 of register 0x0889 (SX126X_REG_SENSITIVITY_CONFIG).
   * Without this the TX signal is corrupt/undecodable. */
  {
    uint8_t rdcmd[3] = { SX1262_READ_REGISTER, 0x08, 0x89 };
    uint8_t rsp[2] = {0};
    cmd_read(rdcmd, sizeof(rdcmd), rsp, sizeof(rsp));
    uint8_t regv = rsp[1];
    uint8_t wrcmd[4] = { SX1262_WRITE_REGISTER, 0x08, 0x89, (uint8_t)(regv | 0x04) };
    cmd_write(wrcmd, sizeof(wrcmd));
  }

  /* RX boosted gain: SX126X_REG_RXGAIN (0x08AC) = 0x96 */
  {
    uint8_t cmd[4] = { SX1262_WRITE_REGISTER, 0x08, 0xAC, 0x96 };
    cmd_write(cmd, sizeof(cmd));
  }

  uint8_t base[3] = { SX1262_SET_BUFFER_BASE_ADDRESS, 0x00, 0x00 };
  if (cmd_write(base, sizeof(base)) != 0) return -1;

  uint8_t irq[9] = {
    SX1262_SET_DIO_IRQ_PARAMS,
    0x02, 0x43,
    0x02, 0x43,
    0x00, 0x00,
    0x00, 0x00,
  };
  if (cmd_write(irq, sizeof(irq)) != 0) return -1;

  return 0;
}

int gdfu_radio_set_tx_power(uint8_t dbm)
{
  if (dbm < GDFU_MIN_TX_POWER_DBM || dbm > GDFU_MAX_TX_POWER_DBM) return -1;
  const uint8_t tx_params[3] = { SX1262_SET_TX_PARAMS, dbm, 0x04 };
  return cmd_write(tx_params, sizeof(tx_params));
}

int gdfu_radio_send(const uint8_t *buf, size_t len, uint32_t timeout_ms)
{
  if (len > 255) return -1;

  uint8_t standby[2] = { SX1262_SET_STANDBY, 0x00 };
  cmd_write(standby, sizeof(standby));

  uint8_t pkt_params[10] = {
    SX1262_SET_PACKET_PARAMS,
    0x00, 0x20,
    0x05,
    0x10,
    0x00,
    0x01,
    (uint8_t)len,
    0x02,
    0x01,
  };
  cmd_write(pkt_params, sizeof(pkt_params));

  uint8_t clr[3] = { SX1262_CLEAR_IRQ_STATUS, 0xFF, 0xFF };
  cmd_write(clr, sizeof(clr));

  uint8_t tx_buf[258];
  tx_buf[0] = SX1262_WRITE_BUFFER;
  tx_buf[1] = 0x00;
  for (size_t i = 0; i < len; ++i) tx_buf[2 + i] = buf[i];
  if (cmd_write(tx_buf, len + 2) != 0) return -1;

  front_end_rx(false);
  uint8_t tx_cmd[4] = { SX1262_SET_TX, 0x00, 0x00, 0x00 };
  if (cmd_write(tx_cmd, sizeof(tx_cmd)) != 0) return -1;

  while (timeout_ms > 0)
  {
    if (nrf_gpio_pin_read(SX1262_DIO1_PIN))
    {
      cmd_write(clr, sizeof(clr));
      cmd_write(standby, sizeof(standby));
      return 0;
    }
    nrf_delay_ms(1);
    timeout_ms--;
  }
  cmd_write(standby, sizeof(standby));
  return -1;
}

int gdfu_radio_recv(uint8_t *buf, size_t cap, size_t *out_len, uint32_t timeout_ms)
{
  uint8_t standby[2] = { SX1262_SET_STANDBY, 0x00 };
  cmd_write(standby, sizeof(standby));

  uint8_t pkt_params[10] = {
    SX1262_SET_PACKET_PARAMS,
    0x00, 0x20,
    0x05,
    0x10,
    0x00,
    0x01,
    0xFF,
    0x02,
    0x01,
  };
  cmd_write(pkt_params, sizeof(pkt_params));

  uint8_t clr[3] = { SX1262_CLEAR_IRQ_STATUS, 0xFF, 0xFF };
  cmd_write(clr, sizeof(clr));

  front_end_rx(true);
  uint8_t rx_cmd[4] = { SX1262_SET_RX, 0xFF, 0xFF, 0xFF };
  if (cmd_write(rx_cmd, sizeof(rx_cmd)) != 0) return -1;

  while (timeout_ms > 0)
  {
    if (nrf_gpio_pin_read(SX1262_DIO1_PIN))
    {
      cmd_write(clr, sizeof(clr));

      uint8_t rcmd[1] = { SX1262_GET_RX_BUFFER_STATUS };
      uint8_t rsp[3] = {0};
      if (cmd_read(rcmd, sizeof(rcmd), rsp, sizeof(rsp)) != 0)
      {
        cmd_write(standby, sizeof(standby));
        return -1;
      }
      uint8_t plen = rsp[1];
      uint8_t offset = rsp[2];
      if (plen == 0 || plen > cap)
      {
        cmd_write(standby, sizeof(standby));
        return -1;
      }

      uint8_t rbuf_cmd[2] = { SX1262_READ_BUFFER, offset };
      if (cmd_read(rbuf_cmd, sizeof(rbuf_cmd), s_spi_rx_buf, plen + 1) != 0)
      {
        cmd_write(standby, sizeof(standby));
        return -1;
      }
      memcpy(buf, &s_spi_rx_buf[1], plen);
      *out_len = plen;
      cmd_write(standby, sizeof(standby));
      return 0;
    }
    nrf_delay_ms(1);
    timeout_ms--;
  }
  cmd_write(standby, sizeof(standby));
  return -1;
}

void gdfu_radio_recover(void)
{
  gdfu_radio_init();
}

void gdfu_radio_sleep(void)
{
  front_end_rx(false);
  uint8_t standby[2] = { SX1262_SET_STANDBY, 0x00 };
  cmd_write(standby, sizeof(standby));
}
