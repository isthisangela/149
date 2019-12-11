// LoRa 9x_RX
// Example sketch showing how to create a simple messaging client (receiver)
// with the RH_RF95 class. RH_RF95 class does not provide for addressing or
// reliability, so you should only use RH_RF95 if you do not need the higher
// level messaging abilities.
// It is designed to work with the other example LoRa9x_RX
 
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <pthread.h>
#include <time.h>

#include "dwm_api.h"
#include "test_util.h"
#include "nrf_drv_gpiote.h"
#include "app_error.h"
#include "app_timer.h"
#include "nrf.h"
#include "nrf_delay.h"
#include "nrf_gpio.h"
#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_drv_spi.h"


// Register names (LoRa Mode, from table 85)
#define RH_RF95_REG_00_FIFO                                0x00
#define RH_RF95_REG_01_OP_MODE                             0x01
#define RH_RF95_REG_02_RESERVED                            0x02
#define RH_RF95_REG_03_RESERVED                            0x03
#define RH_RF95_REG_04_RESERVED                            0x04
#define RH_RF95_REG_05_RESERVED                            0x05
#define RH_RF95_REG_06_FRF_MSB                             0x06
#define RH_RF95_REG_07_FRF_MID                             0x07
#define RH_RF95_REG_08_FRF_LSB                             0x08
#define RH_RF95_REG_09_PA_CONFIG                           0x09
#define RH_RF95_REG_0A_PA_RAMP                             0x0a
#define RH_RF95_REG_0B_OCP                                 0x0b
#define RH_RF95_REG_0C_LNA                                 0x0c
#define RH_RF95_REG_0D_FIFO_ADDR_PTR                       0x0d
#define RH_RF95_REG_0E_FIFO_TX_BASE_ADDR                   0x0e
#define RH_RF95_REG_0F_FIFO_RX_BASE_ADDR                   0x0f
#define RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR                0x10
#define RH_RF95_REG_11_IRQ_FLAGS_MASK                      0x11
#define RH_RF95_REG_12_IRQ_FLAGS                           0x12
#define RH_RF95_REG_13_RX_NB_BYTES                         0x13
#define RH_RF95_REG_14_RX_HEADER_CNT_VALUE_MSB             0x14
#define RH_RF95_REG_15_RX_HEADER_CNT_VALUE_LSB             0x15
#define RH_RF95_REG_16_RX_PACKET_CNT_VALUE_MSB             0x16
#define RH_RF95_REG_17_RX_PACKET_CNT_VALUE_LSB             0x17
#define RH_RF95_REG_18_MODEM_STAT                          0x18
#define RH_RF95_REG_19_PKT_SNR_VALUE                       0x19
#define RH_RF95_REG_1A_PKT_RSSI_VALUE                      0x1a
#define RH_RF95_REG_1B_RSSI_VALUE                          0x1b
#define RH_RF95_REG_1C_HOP_CHANNEL                         0x1c
#define RH_RF95_REG_1D_MODEM_CONFIG1                       0x1d
#define RH_RF95_REG_1E_MODEM_CONFIG2                       0x1e
#define RH_RF95_REG_1F_SYMB_TIMEOUT_LSB                    0x1f
#define RH_RF95_REG_20_PREAMBLE_MSB                        0x20
#define RH_RF95_REG_21_PREAMBLE_LSB                        0x21
#define RH_RF95_REG_22_PAYLOAD_LENGTH                      0x22
#define RH_RF95_REG_23_MAX_PAYLOAD_LENGTH                  0x23
#define RH_RF95_REG_24_HOP_PERIOD                          0x24
#define RH_RF95_REG_25_FIFO_RX_BYTE_ADDR                   0x25
#define RH_RF95_REG_26_MODEM_CONFIG3                       0x26

#define RH_RF95_REG_27_PPM_CORRECTION                      0x27
#define RH_RF95_REG_28_FEI_MSB                             0x28
#define RH_RF95_REG_29_FEI_MID                             0x29
#define RH_RF95_REG_2A_FEI_LSB                             0x2a
#define RH_RF95_REG_2C_RSSI_WIDEBAND                       0x2c
#define RH_RF95_REG_31_DETECT_OPTIMIZE                     0x31
#define RH_RF95_REG_33_INVERT_IQ                           0x33
#define RH_RF95_REG_37_DETECTION_THRESHOLD                 0x37
#define RH_RF95_REG_39_SYNC_WORD                           0x39

#define RH_RF95_REG_40_DIO_MAPPING1                        0x40
#define RH_RF95_REG_41_DIO_MAPPING2                        0x41
#define RH_RF95_REG_42_VERSION                             0x42

#define RH_RF95_REG_4B_TCXO                                0x4b
#define RH_RF95_REG_4D_PA_DAC                              0x4d
#define RH_RF95_REG_5B_FORMER_TEMP                         0x5b
#define RH_RF95_REG_61_AGC_REF                             0x61
#define RH_RF95_REG_62_AGC_THRESH1                         0x62
#define RH_RF95_REG_63_AGC_THRESH2                         0x63
#define RH_RF95_REG_64_AGC_THRESH3                         0x64

// RH_RF95_REG_01_OP_MODE                             0x01
#define RH_RF95_LONG_RANGE_MODE                       0x80
#define RH_RF95_ACCESS_SHARED_REG                     0x40
#define RH_RF95_LOW_FREQUENCY_MODE                    0x08
#define RH_RF95_MODE                                  0x07
#define RH_RF95_MODE_SLEEP                            0x00
#define RH_RF95_MODE_STDBY                            0x01
#define RH_RF95_MODE_FSTX                             0x02
#define RH_RF95_MODE_TX                               0x03
#define RH_RF95_MODE_FSRX                             0x04
#define RH_RF95_MODE_RXCONTINUOUS                     0x05
#define RH_RF95_MODE_RXSINGLE                         0x06
#define RH_RF95_MODE_CAD                              0x07

// RH_RF95_REG_09_PA_CONFIG                           0x09
#define RH_RF95_PA_SELECT                             0x80
#define RH_RF95_MAX_POWER                             0x70
#define RH_RF95_OUTPUT_POWER                          0x0f

// RH_RF95_REG_0A_PA_RAMP                             0x0a
#define RH_RF95_LOW_PN_TX_PLL_OFF                     0x10
#define RH_RF95_PA_RAMP                               0x0f
#define RH_RF95_PA_RAMP_3_4MS                         0x00
#define RH_RF95_PA_RAMP_2MS                           0x01
#define RH_RF95_PA_RAMP_1MS                           0x02
#define RH_RF95_PA_RAMP_500US                         0x03
#define RH_RF95_PA_RAMP_250US                         0x04
#define RH_RF95_PA_RAMP_125US                         0x05
#define RH_RF95_PA_RAMP_100US                         0x06
#define RH_RF95_PA_RAMP_62US                          0x07
#define RH_RF95_PA_RAMP_50US                          0x08
#define RH_RF95_PA_RAMP_40US                          0x09
#define RH_RF95_PA_RAMP_31US                          0x0a
#define RH_RF95_PA_RAMP_25US                          0x0b
#define RH_RF95_PA_RAMP_20US                          0x0c
#define RH_RF95_PA_RAMP_15US                          0x0d
#define RH_RF95_PA_RAMP_12US                          0x0e
#define RH_RF95_PA_RAMP_10US                          0x0f

// RH_RF95_REG_0B_OCP                                 0x0b
#define RH_RF95_OCP_ON                                0x20
#define RH_RF95_OCP_TRIM                              0x1f

// RH_RF95_REG_0C_LNA                                 0x0c
#define RH_RF95_LNA_GAIN                              0xe0
#define RH_RF95_LNA_GAIN_G1                           0x20
#define RH_RF95_LNA_GAIN_G2                           0x40
#define RH_RF95_LNA_GAIN_G3                           0x60                
#define RH_RF95_LNA_GAIN_G4                           0x80
#define RH_RF95_LNA_GAIN_G5                           0xa0
#define RH_RF95_LNA_GAIN_G6                           0xc0
#define RH_RF95_LNA_BOOST_LF                          0x18
#define RH_RF95_LNA_BOOST_LF_DEFAULT                  0x00
#define RH_RF95_LNA_BOOST_HF                          0x03
#define RH_RF95_LNA_BOOST_HF_DEFAULT                  0x00
#define RH_RF95_LNA_BOOST_HF_150PC                    0x03

// RH_RF95_REG_11_IRQ_FLAGS_MASK                      0x11
#define RH_RF95_RX_TIMEOUT_MASK                       0x80
#define RH_RF95_RX_DONE_MASK                          0x40
#define RH_RF95_PAYLOAD_CRC_ERROR_MASK                0x20
#define RH_RF95_VALID_HEADER_MASK                     0x10
#define RH_RF95_TX_DONE_MASK                          0x08
#define RH_RF95_CAD_DONE_MASK                         0x04
#define RH_RF95_FHSS_CHANGE_CHANNEL_MASK              0x02
#define RH_RF95_CAD_DETECTED_MASK                     0x01

// RH_RF95_REG_12_IRQ_FLAGS                           0x12
#define RH_RF95_RX_TIMEOUT                            0x80
#define RH_RF95_RX_DONE                               0x40
#define RH_RF95_PAYLOAD_CRC_ERROR                     0x20
#define RH_RF95_VALID_HEADER                          0x10
#define RH_RF95_TX_DONE                               0x08
#define RH_RF95_CAD_DONE                              0x04
#define RH_RF95_FHSS_CHANGE_CHANNEL                   0x02
#define RH_RF95_CAD_DETECTED                          0x01

// RH_RF95_REG_18_MODEM_STAT                          0x18
#define RH_RF95_RX_CODING_RATE                        0xe0
#define RH_RF95_MODEM_STATUS_CLEAR                    0x10
#define RH_RF95_MODEM_STATUS_HEADER_INFO_VALID        0x08
#define RH_RF95_MODEM_STATUS_RX_ONGOING               0x04
#define RH_RF95_MODEM_STATUS_SIGNAL_SYNCHRONIZED      0x02
#define RH_RF95_MODEM_STATUS_SIGNAL_DETECTED          0x01

// RH_RF95_REG_1C_HOP_CHANNEL                         0x1c
#define RH_RF95_PLL_TIMEOUT                           0x80
#define RH_RF95_RX_PAYLOAD_CRC_IS_ON                  0x40
#define RH_RF95_FHSS_PRESENT_CHANNEL                  0x3f

// RH_RF95_REG_1D_MODEM_CONFIG1                       0x1d
#define RH_RF95_BW                                    0xf0

#define RH_RF95_BW_7_8KHZ                             0x00
#define RH_RF95_BW_10_4KHZ                            0x10
#define RH_RF95_BW_15_6KHZ                            0x20
#define RH_RF95_BW_20_8KHZ                            0x30
#define RH_RF95_BW_31_25KHZ                           0x40
#define RH_RF95_BW_41_7KHZ                            0x50
#define RH_RF95_BW_62_5KHZ                            0x60
#define RH_RF95_BW_125KHZ                             0x70
#define RH_RF95_BW_250KHZ                             0x80
#define RH_RF95_BW_500KHZ                             0x90
#define RH_RF95_CODING_RATE                           0x0e
#define RH_RF95_CODING_RATE_4_5                       0x02
#define RH_RF95_CODING_RATE_4_6                       0x04
#define RH_RF95_CODING_RATE_4_7                       0x06
#define RH_RF95_CODING_RATE_4_8                       0x08
#define RH_RF95_IMPLICIT_HEADER_MODE_ON               0x01

// RH_RF95_REG_1E_MODEM_CONFIG2                       0x1e
#define RH_RF95_SPREADING_FACTOR                      0xf0
#define RH_RF95_SPREADING_FACTOR_64CPS                0x60
#define RH_RF95_SPREADING_FACTOR_128CPS               0x70
#define RH_RF95_SPREADING_FACTOR_256CPS               0x80
#define RH_RF95_SPREADING_FACTOR_512CPS               0x90
#define RH_RF95_SPREADING_FACTOR_1024CPS              0xa0
#define RH_RF95_SPREADING_FACTOR_2048CPS              0xb0
#define RH_RF95_SPREADING_FACTOR_4096CPS              0xc0
#define RH_RF95_TX_CONTINUOUS_MODE                    0x08

#define RH_RF95_PAYLOAD_CRC_ON                        0x04
#define RH_RF95_SYM_TIMEOUT_MSB                       0x03

// RH_RF95_REG_26_MODEM_CONFIG3
#define RH_RF95_MOBILE_NODE                           0x08 // HopeRF term
#define RH_RF95_LOW_DATA_RATE_OPTIMIZE                0x08 // Semtechs term
#define RH_RF95_AGC_AUTO_ON                           0x04

// RH_RF95_REG_4B_TCXO                                0x4b
#define RH_RF95_TCXO_TCXO_INPUT_ON                    0x10

// RH_RF95_REG_4D_PA_DAC                              0x4d
#define RH_RF95_PA_DAC_DISABLE                        0x04
#define RH_RF95_PA_DAC_ENABLE                         0x07


// SPI
// LoRa Module pin connections
// Reset
#define RFM95_RST   NRF_GPIO_PIN_MAP(0,31)
// Slave/chip select
#define RFM95_CS    NRF_GPIO_PIN_MAP(0,30)
// Master out, slave in
#define SPI_MOSI    NRF_GPIO_PIN_MAP(0,29)
// Master in, slave out
#define SPI_MISO    NRF_GPIO_PIN_MAP(0,28)
// Clock
#define SPI_SCLK    NRF_GPIO_PIN_MAP(0,4)
// Interrupt
#define RFM95_INT   NRF_GPIO_PIN_MAP(0,3)
// Button1 for turning on
#define BUTTON_1    NRF_GPIO_PIN_MAP(0,13)
// Button2 for turning off
#define BUTTON_2    NRF_GPIO_PIN_MAP(0,14)

// SPI
// DWM Module pin connections
// Reset
#define DWM_RST     NRF_GPIO_PIN_MAP(0,27)
// Slave/chip select
#define DWM_CS      NRF_GPIO_PIN_MAP(0,26)
// Master out, slave in
#define DWM_MOSI    NRF_GPIO_PIN_MAP(0,25)
// Master in, slave out
#define DWM_MISO    NRF_GPIO_PIN_MAP(0,24)
// Clock
#define DWM_SCLK    NRF_GPIO_PIN_MAP(0,2)
// Interrupt
#define DWM_INT     NRF_GPIO_PIN_MAP(0,23) 


#define RH_RF95_FIFO_SIZE 255
#define RH_RF95_MAX_PAYLOAD_LEN RH_RF95_FIFO_SIZE

#define RH_RF95_HEADER_LEN 4
#define RH_RF95_REG_00_FIFO                                0x00
#define RH_RF95_REG_01_OP_MODE                             0x01
#define RH_RF95_REG_40_DIO_MAPPING1                        0x40
#define RH_RF95_MODE_TX                               0x03
#define RH_RF95_MODE_STDBY                            0x01
#define RH_RF95_REG_22_PAYLOAD_LENGTH                      0x22
#define RH_RF95_REG_0D_FIFO_ADDR_PTR                       0x0d
#define RH_RF95_MAX_MESSAGE_LEN (RH_RF95_MAX_PAYLOAD_LEN - RH_RF95_HEADER_LEN)
#define RH_BROADCAST_ADDRESS 0xff
#define RH_RF95_MAX_POWER                             0x70
#define RH_RF95_REG_09_PA_CONFIG                           0x09
#define RH_RF95_PA_DAC_ENABLE                         0x07
#define RH_RF95_REG_4D_PA_DAC                              0x4d
#define RH_RF95_PA_DAC_DISABLE                        0x04
#define RH_RF95_PA_SELECT                             0x80
#define RH_RF95_REG_08_FRF_LSB                             0x08
#define RH_RF95_REG_07_FRF_MID                             0x07
#define RH_RF95_REG_06_FRF_MSB                             0x06
#define RH_RF95_REG_0E_FIFO_TX_BASE_ADDR                   0x0e
#define RH_RF95_LONG_RANGE_MODE                       0x80


#define RH_RF95_FXOSC 32000000.0
#define RH_RF95_FSTEP  (RH_RF95_FXOSC / 524288)


// Change to 434.0 or other frequency, must match RX's freq!
#define RF95_FREQ 915.0

//DWM
#define RESP_ERRNO_LEN           3
#define RESP_DAT_TYPE_OFFSET     RESP_ERRNO_LEN
#define RESP_DAT_LEN_OFFSET      RESP_DAT_TYPE_OFFSET+1
#define RESP_DAT_VALUE_OFFSET    RESP_DAT_LEN_OFFSET+1
#define RESP_DATA_LOC_LOC_SIZE     15
#define RESP_DATA_LOC_DIST_OFFSET  RESP_DAT_TYPE_OFFSET + RESP_DATA_LOC_LOC_SIZE
#define RESP_DATA_LOC_DIST_LEN_MIN 3


typedef enum {
  RHModeInitialising = 0, ///< Transport is initialising. Initial default value until init() is called..
  RHModeSleep,            ///< Transport hardware is in low power sleep mode (if supported)
  RHModeIdle,             ///< Transport is idle.
  RHModeTx,               ///< Transport is in the process of transmitting a message.
  RHModeRx,               ///< Transport is in the process of receiving a message.
  RHModeCad               ///< Transport is in the process of detecting channel activity (if supported)
} RHMode;

typedef enum {
  DataRate1Mbps = 0,   ///< 1 Mbps
  DataRate2Mbps,       ///< 2 Mbps
  DataRate250kbps      ///< 250 kbps
} DataRate;

volatile RHMode     _mode;
nrf_drv_spi_config_t spi_config;
nrf_drv_spi_config_t dwm_spi_config;
uint8_t _txHeaderFlags = 0;
volatile uint16_t   _txGood;
volatile uint8_t    _bufLen;
// Last measured SNR, dB
int8_t              _lastSNR; 
volatile int16_t     _lastRssi;
uint8_t _txHeaderId = 0;
uint8_t _txHeaderTo = RH_BROADCAST_ADDRESS;
uint8_t _txHeaderFrom = RH_BROADCAST_ADDRESS;
bool _usingHFport = true;
uint8_t flag = 0;
  
/// Channel activity detected
volatile bool       _cad;
/// TO header in the last received mesasge
volatile uint8_t    _rxHeaderTo;
/// FROM header in the last received mesasge
volatile uint8_t    _rxHeaderFrom;
/// ID header in the last received mesasge
volatile uint8_t    _rxHeaderId;
/// FLAGS header in the last received mesasge
volatile uint8_t    _rxHeaderFlags;
/// Whether the transport is in promiscuous mode
bool                _promiscuous;
/// Count of the number of successfully transmitted messaged
volatile uint16_t   _rxGood;
/// This node id
uint8_t             _thisAddress;
/// Count of the number of bad messages (eg bad checksum etc) received
volatile uint16_t   _rxBad;
uint8_t             _buf[RH_RF95_MAX_PAYLOAD_LEN+1];
/// True when there is a valid message in the buffer
volatile bool       _rxBufValid;


static nrf_drv_spi_t instance = NRF_DRV_SPI_INSTANCE(1);
static const nrf_drv_spi_t* spi_instance;

//for dwm
struct timeval tv;
uint64_t ts_curr = 0;
uint64_t ts_last = 0;
volatile uint8_t data_ready;


void clearRxBuf() {
    _rxBufValid = false;
    _buf[1] = 0;
}

// writes one uint8_t value
void spiWrite(uint8_t reg, uint8_t val) {
  uint8_t buf[257];
  buf[0] = 0x80 | reg;
  memcpy(buf+1, &val, 1);
  nrf_drv_spi_init(spi_instance, &spi_config, NULL, NULL);
  ret_code_t err = nrf_drv_spi_transfer(spi_instance, buf, 2, NULL, 0);
  nrf_drv_spi_uninit(spi_instance);
}

void setModeIdle() {
  if (_mode != RHModeIdle) {
    spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_STDBY);
    _mode = RHModeIdle;
  }
}

void setModeRx()
{
    if (_mode != RHModeRx)
    {
  spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_RXCONTINUOUS);
  spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x00); // Interrupt on RxDone
  _mode = RHModeRx;
    }
}

// Check whether the latest received message is complete and uncorrupted
void validateRxBuf() {
  if (_buf[1] < RH_RF95_HEADER_LEN)
    return; // Too short to be a real message
  // Extract the 4 headers following S0, LEN and S1
  _rxHeaderTo    = _buf[3];
  _rxHeaderFrom  = _buf[4];
  _rxHeaderId    = _buf[5];
  _rxHeaderFlags = _buf[6];

  if (_promiscuous || _rxHeaderTo == _thisAddress || _rxHeaderTo == RH_BROADCAST_ADDRESS) {
    _rxGood++;
    _rxBufValid = true;
  }
}

//bool RH_RF95::available()
bool available()
{
    if (_mode == RHModeTx)
      return false;
    setModeRx();
    return _rxBufValid; // Will be set by the interrupt handler when a good message is received
}


bool recv(uint8_t* buf, uint8_t* len) {
  if (!available())
    return false;
  if (buf && len) {
    // Skip the 4 headers that are at the beginning of the rxBuf
    // the payload length is the first octet in _buf
    if (*len > _buf[1]-RH_RF95_HEADER_LEN)
      *len = _buf[1]-RH_RF95_HEADER_LEN;
    memcpy(buf, _buf+RH_RF95_HEADER_LEN, *len);
  }
  clearRxBuf(); // This message accepted and cleared
  return true;
}

void spiBurstWrite(uint8_t reg, uint8_t* write_buf, size_t len){
  if (len > 256) return;
  uint8_t buf[257];
  buf[0] = 0x80 | reg;
  memcpy(buf+1, write_buf, len);
  nrf_drv_spi_init(spi_instance, &spi_config, NULL, NULL);
  nrf_drv_spi_transfer(spi_instance, buf, len+1, NULL, 0);
  nrf_drv_spi_uninit(spi_instance);
}


// reads a buffer of values
void spiBurstRead(uint8_t reg, uint8_t* read_buf, size_t len){
  if (len > 256) return;
  uint8_t readreg = reg;
  uint8_t buf[257];

  nrf_drv_spi_init(spi_instance, &spi_config, NULL, NULL);
  nrf_drv_spi_transfer(spi_instance, &readreg, 1, buf, len+1);
  nrf_drv_spi_uninit(spi_instance);

  memcpy(read_buf, buf+1, len);
}

// reads one uint8_t value
uint8_t spiRead(uint8_t reg) {
  uint8_t buf[2];
  nrf_drv_spi_init(spi_instance, &spi_config, NULL, NULL);
  ret_code_t err = nrf_drv_spi_transfer(spi_instance, &reg, 1, buf, 2);
  nrf_drv_spi_uninit(spi_instance);
  return buf[1];
}

void setModeTx() {
  if (_mode != RHModeTx) {
    spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_TX);
    spiWrite(RH_RF95_REG_40_DIO_MAPPING1, 0x40); // Interrupt on TxDone
    _mode = RHModeTx;
  }
}

//void RH_RF95::setTxPower(int8_t power, bool useRFO)
void setTxPower(int8_t power, bool useRFO) {
  // Sigh, different behaviours depending on whther the module use PA_BOOST or the RFO pin
  // for the transmitter output
  if (useRFO) {
    if (power > 14)
      power = 14;
    if (power < -1)
      power = -1;
    spiWrite(RH_RF95_REG_09_PA_CONFIG, RH_RF95_MAX_POWER | (power + 1));
  } else {
    if (power > 23)
      power = 23;
    if (power < 5)
      power = 5;
    // For RH_RF95_PA_DAC_ENABLE, manual says '+20dBm on PA_BOOST when OutputPower=0xf'
    // RH_RF95_PA_DAC_ENABLE actually adds about 3dBm to all power levels. We will us it
    // for 21, 22 and 23dBm
    if (power > 20) {
      spiWrite(RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_ENABLE);
      power -= 3;
    } else {
      spiWrite(RH_RF95_REG_4D_PA_DAC, RH_RF95_PA_DAC_DISABLE);
    }
    // RFM95/96/97/98 does not have RFO pins connected to anything. Only PA_BOOST
    // pin is connected, so must use PA_BOOST
    // Pout = 2 + OutputPower.
    // The documentation is pretty confusing on this topic: PaSelect says the max power is 20dBm,
    // but OutputPower claims it would be 17dBm.
    // My measurements show 20dBm is correct
    spiWrite(RH_RF95_REG_09_PA_CONFIG, RH_RF95_PA_SELECT | (power-5));
  }
}

bool setFrequency(float centre) {
  // Frf = FRF / FSTEP
  uint32_t frf = (centre * 1000000.0) / RH_RF95_FSTEP;
  spiWrite(RH_RF95_REG_06_FRF_MSB, (frf >> 16) & 0xff);
  spiWrite(RH_RF95_REG_07_FRF_MID, (frf >> 8) & 0xff);
  spiWrite(RH_RF95_REG_08_FRF_LSB, frf & 0xff);
  _usingHFport = (centre >= 779.0);

  return true;
}

bool waitPacketSent() {
  while (_mode == RHModeTx)
    ;
    //pthread_yield(); // Wait for any previous transmit to finish
  return true;
}


void handleInterrupts(nrf_drv_gpiote_pin_t pin, nrf_gpiote_polarity_t action) {
  // Read the interrupt register
  uint8_t irq_flags = spiRead(RH_RF95_REG_12_IRQ_FLAGS);
  // Read the RegHopChannel register to check if CRC presence is signalled
  // in the header. If not it might be a stray (noise) packet.*
  //uint8_t crc_present = spiRead(RH_RF95_REG_1C_HOP_CHANNEL);

  if (_mode == RHModeRx && irq_flags & RH_RF95_RX_DONE) {
    // Have received a packet
    uint8_t len = spiRead(RH_RF95_REG_13_RX_NB_BYTES);

    //  Reset the fifo read ptr to the beginning of the packet
    spiWrite(RH_RF95_REG_0D_FIFO_ADDR_PTR, spiRead(RH_RF95_REG_10_FIFO_RX_CURRENT_ADDR));
    spiBurstRead(RH_RF95_REG_00_FIFO, _buf, len);
    _bufLen = len;
    spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags

    // We have received a message.
    validateRxBuf(); 
    if (_rxBufValid)
      setModeIdle(); // Got one 
  } else if (_mode == RHModeTx && irq_flags & RH_RF95_TX_DONE) {
    _txGood++;
    setModeIdle();
  } else if (_mode == RHModeCad && irq_flags & RH_RF95_CAD_DONE) {
    _cad = irq_flags & RH_RF95_CAD_DETECTED;
    setModeIdle();
  }
    // Sigh: on some processors, for some unknown reason, doing this only once does not actually
    // clear the radio's interrupt flag. So we do it twice. Why?
    spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags
    spiWrite(RH_RF95_REG_12_IRQ_FLAGS, 0xff); // Clear all IRQ flags
}


void setPreambleLength(uint16_t bytes) {
    spiWrite(RH_RF95_REG_20_PREAMBLE_MSB, bytes >> 8);
    spiWrite(RH_RF95_REG_21_PREAMBLE_LSB, bytes & 0xff);
}

//bool RH_RF95::init()
bool init() {
    spiWrite(RH_RF95_REG_01_OP_MODE, RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE);
    nrf_delay_ms(10); // Wait for sleep mode to take over from say, CAD
    // Check we are in sleep mode, with LORA set
    if (spiRead(RH_RF95_REG_01_OP_MODE) != (RH_RF95_MODE_SLEEP | RH_RF95_LONG_RANGE_MODE)) {
      return false; // No device present?
    }

    // Set up FIFO
    // We configure so that we can use the entire 256 byte FIFO for either receive
    // or transmit, but not both at the same time
    spiWrite(RH_RF95_REG_0E_FIFO_TX_BASE_ADDR, 0);
    spiWrite(RH_RF95_REG_0F_FIFO_RX_BASE_ADDR, 0);

    // Packet format is preamble + explicit-header + payload + crc
    // Explicit Header Mode
    // payload is TO + FROM + ID + FLAGS + message data
    // RX mode is implmented with RXCONTINUOUS
    // max message data length is 255 - 4 = 251 octets

    setModeIdle();

    // Set up default configuration
    // No Sync Words in LORA mode.

    //setModemConfig(0); // Radio default
//    setModemConfig(Bw125Cr48Sf4096); // slow and reliable?
    setPreambleLength(8); // Default is 8
    // An innocuous ISM frequency, same as RF22's
    setFrequency(915.0);
    // Lowish power
    setTxPower(13, false);

    return true;
}
 
bool send(const uint8_t* data, uint8_t len) {
  if (len > RH_RF95_MAX_MESSAGE_LEN)
    return false;

  waitPacketSent(); // Make sure we dont interrupt an outgoing message
  setModeIdle();
  // Position at the beginning of the FIFO
  spiWrite(RH_RF95_REG_0D_FIFO_ADDR_PTR, 0);
  // The headers
  spiWrite(RH_RF95_REG_00_FIFO, _txHeaderTo);
  spiWrite(RH_RF95_REG_00_FIFO, _txHeaderFrom);
  spiWrite(RH_RF95_REG_00_FIFO, _txHeaderId);
  spiWrite(RH_RF95_REG_00_FIFO, _txHeaderFlags);
  // The message data
  spiBurstWrite(RH_RF95_REG_00_FIFO, data, len);
  spiWrite(RH_RF95_REG_22_PAYLOAD_LENGTH, len + RH_RF95_HEADER_LEN);

  setModeTx(); // Start the transmitter
  printf("TxDone\n");
  // when Tx is done, interruptHandler will fire and radio mode will return to STANDBY
  return true;
}

bool waitAvailableTimeout(uint16_t timeout) {
  uint16_t counter = 0;
  while (counter < timeout) {
    counter += 200;
    nrf_delay_ms(200);
    printf("waiting... \n");
    if (available()) {
      printf("true\n");
        return true;
    }
    //pthread_yield();  
  }
  return false;
}

int16_t packetnum = 0;  // packet counter, we increment per xmission

void loop() {
  printf("Sending to rf95_server\n");
  // Send a message to rf95_server
  
  char radiopacket[20] = "Bello World #      ";
  itoa(packetnum++, radiopacket+13, 10);
  radiopacket[19] = 0;
  
  printf("Sending...\n");
  nrf_delay_ms(10);
  send((uint8_t *)radiopacket, 20);
 
  printf("Waiting for packet to complete...\n");
  nrf_delay_ms(10);
  waitPacketSent();
  // Now wait for a reply
  uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
  uint8_t len = sizeof(buf);
 
  printf("Waiting for reply...\n");
  //nrf_delay_ms(500);
  if (waitAvailableTimeout(1000)) { 
    // Should be a reply message for us now   
    if (recv(buf, &len)) {
      printf("Got reply:");
      printf("%s\n", buf);    }
    else {
      printf("Receive failed\n");
    }
  }
  else {
    printf("No reply, is there a listener around?\n");
  }
  //nrf_delay_ms(1000);
}

void loop_button_on() {
  
    printf("Sending 'turn on' to rf95_server\n");
    // Send a message to rf95_server
  
    char radiopacket[20] = "turn on     #";
    itoa(packetnum++, radiopacket+13, 10);
    radiopacket[19] = 0;
  
    printf("Sending...\n");
    nrf_delay_ms(10);
    send((uint8_t *)radiopacket, 20);
 
    printf("Waiting for packet to complete...\n");
    nrf_delay_ms(10);
    waitPacketSent();
    // Now wait for a reply
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
 
    printf("Waiting for reply...\n");
    //nrf_delay_ms(500);
    if (waitAvailableTimeout(1000)) { 
      // Should be a reply message for us now   
      if (recv(buf, &len)) {
          printf("Got reply:");
          printf("%s\n", buf);   
          flag = 0;
          return;
        }
      else {
          printf("Receive failed\n");
      }
    }
    else {
      printf("No reply, is there a listener around?\n");
    }
    //nrf_delay_ms(1000);
  
}

void loop_button_off() {
  
    printf("Sending 'turn off' to rf95_server\n");
    // Send a message to rf95_server
  
    char radiopacket[20] = "turn off     #";
    itoa(packetnum++, radiopacket+13, 10);
    radiopacket[19] = 0;
  
    printf("Sending...\n");
    nrf_delay_ms(10);
    send((uint8_t *)radiopacket, 20);
 
    printf("Waiting for packet to complete...\n");
    nrf_delay_ms(10);
    waitPacketSent();
    // Now wait for a reply
    uint8_t buf[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t len = sizeof(buf);
 
    printf("Waiting for reply...\n");
    nrf_delay_ms(500);
    if (waitAvailableTimeout(1000)) { 
      // Should be a reply message for us now   
      if (recv(buf, &len)) {
          printf("Got reply:");
          printf("%s\n", buf);   
          flag = 0;
          return;
        }
      else {
          printf("Receive failed\n");
      }
    }
    else {
      printf("No reply, is there a listener around?\n");
    }
    //nrf_delay_ms(1000);
  }


void button_on() {
  flag = 1;
}

void button_off() {
  flag = 2;
}

void location_ready() {
  data_ready = 1;
}

static void gpio_init(void) {
    ret_code_t err_code;

    err_code = nrf_drv_gpiote_init();
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_config_t in_config_G0 = GPIOTE_CONFIG_IN_SENSE_LOTOHI(true);
    in_config_G0.pull = NRF_GPIO_PIN_PULLUP;

    err_code = nrf_drv_gpiote_in_init(RFM95_INT, &in_config_G0, handleInterrupts);
    err_code = nrf_drv_gpiote_in_init(DWM_INT, &in_config_G0, location_ready);
    APP_ERROR_CHECK(err_code);

    nrf_drv_gpiote_in_config_t in_config_Button = GPIOTE_CONFIG_IN_SENSE_HITOLO(true);
    in_config_Button.pull = NRF_GPIO_PIN_PULLUP;

    err_code = nrf_drv_gpiote_in_init(BUTTON_1, &in_config_Button, button_on);
    err_code = nrf_drv_gpiote_in_init(BUTTON_2, &in_config_Button, button_off);
    APP_ERROR_CHECK(err_code);





    nrf_drv_gpiote_in_event_enable(RFM95_INT, true);
    nrf_drv_gpiote_in_event_enable(BUTTON_1, true);
    nrf_drv_gpiote_in_event_enable(BUTTON_2, true);
    nrf_drv_gpiote_in_event_enable(DWM_INT, true);

}

void update_message(uint8_t *msg, size_t msg_len) {
	printf("length %d\n", msg_len);
  for (size_t i = 0; i < msg_len; i++) {
    printf("%x\n", msg[i]);
    msg[i] = msg[i] << 1;
    printf("%x\n", msg[i]);
  }
}


int dwm_loc_get(dwm_loc_data_t* loc)
{ 
   uint8_t tx_data[DWM1001_TLV_MAX_SIZE], tx_len = 0;
   uint8_t rx_data[DWM1001_TLV_MAX_SIZE];
   uint16_t rx_len;
   uint8_t data_cnt, i, j;
   uint8_t waittime = 5;
   tx_data[tx_len++] = DWM1001_TLV_TYPE_CMD_LOC_GET;
   tx_data[tx_len++] = 0;   
   // LMH_Tx(tx_data, &tx_len);   
   nrf_drv_spi_init(spi_instance, &dwm_spi_config, NULL, NULL);
   ret_code_t err = nrf_drv_spi_transfer(spi_instance, tx_data, tx_len, NULL, 0);

   err = nrf_drv_spi_transfer(spi_instance, NULL, 0, rx_data, DWM1001_TLV_MAX_SIZE);
   rx_len = sizeof(rx_data)/sizeof(uint8_t);
   while(rx_len <= 0 && waittime > 0){
      err = nrf_drv_spi_transfer(spi_instance, NULL, 0, rx_data, DWM1001_TLV_MAX_SIZE);
      rx_len = sizeof(rx_data)/sizeof(uint8_t);
      waittime -= 1;
      nrf_delay_ms(100);
   }
   
   printf("rx_len: %d and rx_data: %x",rx_len, *rx_data);
   printf("\n");

   // if(LMH_WaitForRx(rx_data, &rx_len, DWM1001_TLV_MAX_SIZE) == RV_OK)
   if(err == NRF_SUCCESS)
   {
      if(rx_len<RESP_ERRNO_LEN+RESP_DATA_LOC_LOC_SIZE + RESP_DATA_LOC_DIST_LEN_MIN)// ok + pos + distance/range
      {
      	 
         nrf_drv_spi_uninit(spi_instance);
         return RV_ERR;
      }
      
      if(rx_data[RESP_DAT_TYPE_OFFSET]==DWM1001_TLV_TYPE_POS_XYZ)//0x41
      {
         // node self position.
         data_cnt = RESP_DAT_VALUE_OFFSET;// jump Type and Length, goto data
         loc->p_pos->x = rx_data[data_cnt] 
                      + (rx_data[data_cnt+1]<<8) 
                      + (rx_data[data_cnt+2]<<16) 
                      + (rx_data[data_cnt+3]<<24); 
         data_cnt += 4;
         loc->p_pos->y = rx_data[data_cnt] 
                      + (rx_data[data_cnt+1]<<8) 
                      + (rx_data[data_cnt+2]<<16) 
                      + (rx_data[data_cnt+3]<<24); 
         data_cnt += 4;
         loc->p_pos->z = rx_data[data_cnt] 
                      + (rx_data[data_cnt+1]<<8) 
                      + (rx_data[data_cnt+2]<<16) 
                      + (rx_data[data_cnt+3]<<24); 
         data_cnt += 4;
         loc->p_pos->qf = rx_data[data_cnt++];
      }
      
      if(rx_data[RESP_DATA_LOC_DIST_OFFSET]==DWM1001_TLV_TYPE_RNG_AN_DIST)//0x48
      {
         // node is Anchor, recording Tag ID, distances and qf
         loc->anchors.dist.cnt = rx_data[RESP_DATA_LOC_DIST_OFFSET+2];
         loc->anchors.an_pos.cnt = 0;
         data_cnt = RESP_DATA_LOC_DIST_OFFSET + 3; // jump Type, Length and cnt, goto data
         for (i = 0; i < loc->anchors.dist.cnt; i++)
         {
            // Tag ID
            loc->anchors.dist.addr[i] = 0;
            for (j = 0; j < 8; j++)
            {
               loc->anchors.dist.addr[i] += rx_data[data_cnt++]<<(j*8);
            }
            // Tag distance
            loc->anchors.dist.dist[i] = 0;
            for (j = 0; j < 4; j++)
            {
               loc->anchors.dist.dist[i] += rx_data[data_cnt++]<<(j*8);
            }
            // Tag qf
            loc->anchors.dist.qf[i] = rx_data[data_cnt++];
         }
      }
      else if (rx_data[RESP_DATA_LOC_DIST_OFFSET]==DWM1001_TLV_TYPE_RNG_AN_POS_DIST)//0x49
      {
         // node is Tag, recording Anchor ID, distances, qf and positions
         loc->anchors.dist.cnt = rx_data[RESP_DATA_LOC_DIST_OFFSET+2];
         loc->anchors.an_pos.cnt = rx_data[RESP_DATA_LOC_DIST_OFFSET+2];
         data_cnt = RESP_DATA_LOC_DIST_OFFSET + 3; // jump Type, Length and cnt, goto data
         for (i = 0; i < loc->anchors.dist.cnt; i++)
         {
            // anchor ID
            loc->anchors.dist.addr[i] = 0;
            for (j = 0; j < 2; j++)
            {
               loc->anchors.dist.addr[i] += ((uint64_t)rx_data[data_cnt++])<<(j*8);
            }
            // anchor distance
            loc->anchors.dist.dist[i] = 0;
            for (j = 0; j < 4; j++)
            {
               loc->anchors.dist.dist[i] += ((uint32_t)rx_data[data_cnt++])<<(j*8);
            }
            // anchor qf
            loc->anchors.dist.qf[i] = rx_data[data_cnt++];
            // anchor position
            loc->anchors.an_pos.pos[i].x  = rx_data[data_cnt] 
                                         + (rx_data[data_cnt+1]<<8) 
                                         + (rx_data[data_cnt+2]<<16) 
                                         + (rx_data[data_cnt+3]<<24); 
            data_cnt += 4;
            loc->anchors.an_pos.pos[i].y = rx_data[data_cnt] 
                                         + (rx_data[data_cnt+1]<<8) 
                                         + (rx_data[data_cnt+2]<<16) 
                                         + (rx_data[data_cnt+3]<<24); 
            data_cnt += 4;
            loc->anchors.an_pos.pos[i].z = rx_data[data_cnt] 
                                         + (rx_data[data_cnt+1]<<8) 
                                         + (rx_data[data_cnt+2]<<16) 
                                         + (rx_data[data_cnt+3]<<24); 
            data_cnt += 4;
            loc->anchors.an_pos.pos[i].qf = rx_data[data_cnt++];
         }
      }
      else
      {
      	
         nrf_drv_spi_uninit(spi_instance);
         return RV_ERR;   
      }
   }
   else
   {

      nrf_drv_spi_uninit(spi_instance);
      return RV_ERR;   
   }
   nrf_drv_spi_uninit(spi_instance);
   return RV_OK;
}



int get_loc(void)
{
   // ========== dwm_loc_get ==========
   int rv, err_cnt = 0, i; 
   
   dwm_loc_data_t loc;
   dwm_pos_t pos;
   loc.p_pos = &pos;
   printf("dwm_loc_get(&loc)\n");
   rv = Test_CheckTxRx(dwm_loc_get(&loc));
   
   gettimeofday(&tv,NULL);      
      
   if(rv == RV_OK)
   {
      printf("ts:%ld.%06ld [%d,%d,%d,%u]\n", tv.tv_sec, tv.tv_usec, loc.p_pos->x, loc.p_pos->y, loc.p_pos->z, loc.p_pos->qf);
      printf("ts:%ld.%06ld [%d,%d,%d,%u]", tv.tv_sec, tv.tv_usec, loc.p_pos->x, loc.p_pos->y, loc.p_pos->z, loc.p_pos->qf);

      for (i = 0; i < loc.anchors.dist.cnt; ++i) 
      {
         // HAL_Log("#%u)", i);
         printf("#%u)", i);
         // HAL_Log("a:0x%08x", loc.anchors.dist.addr[i]);
         printf("a:0x%08x", loc.anchors.dist.addr[i]);
         if (i < loc.anchors.an_pos.cnt) 
         {
            // HAL_Log("[%d,%d,%d,%u]", loc.anchors.an_pos.pos[i].x,
            //       loc.anchors.an_pos.pos[i].y,
            //       loc.anchors.an_pos.pos[i].z,
            //       loc.anchors.an_pos.pos[i].qf);
            printf("[%d,%d,%d,%u]", loc.anchors.an_pos.pos[i].x,
                  loc.anchors.an_pos.pos[i].y,
                  loc.anchors.an_pos.pos[i].z,
                  loc.anchors.an_pos.pos[i].qf);
         }
         // HAL_Log("d=%u,qf=%u\n", loc.anchors.dist.dist[i], loc.anchors.dist.qf[i]);
         printf("d=%u,qf=%u", loc.anchors.dist.dist[i], loc.anchors.dist.qf[i]);
      }
         // HAL_Log("\n");
         printf("\n");
   }
   err_cnt += rv;   
   
   return err_cnt;
}

int dwm_int_cfg_set(uint16_t value)
{        
   uint8_t tx_data[DWM1001_TLV_MAX_SIZE], tx_len = 0;
   uint8_t rx_data[DWM1001_TLV_MAX_SIZE];
   uint16_t rx_len;
   uint8_t waittime = 5;
   tx_data[tx_len++] = DWM1001_TLV_TYPE_CMD_INT_CFG_SET;
   tx_data[tx_len++] = 2;
   tx_data[tx_len++] = value & 0xff;    
   tx_data[tx_len++] = (value>>8) & 0xff;   
   //LMH_Tx(tx_data, &tx_len);   
   nrf_drv_spi_init(spi_instance, &dwm_spi_config, NULL, NULL);
   ret_code_t err = nrf_drv_spi_transfer(spi_instance, tx_data, tx_len, NULL, 0);

   err = nrf_drv_spi_transfer(spi_instance, NULL, 0, rx_data, 3);
   while(err != NRF_SUCCESS && waittime > 0){
   	err = nrf_drv_spi_transfer(spi_instance, NULL, 0, rx_data, 3);
   	waittime -= 1;
   	nrf_delay_ms(100);
   }
   nrf_drv_spi_uninit(spi_instance);
   //return LMH_WaitForRx(rx_data, &rx_len, 3);
   return (err == NRF_SUCCESS);
}



void spi_les_test(void)
{   
   // int err_cnt = 0;  
   // uint16_t panid;
   // uint16_t ur_set;
   // uint16_t ur_s_set;
   
   //initializing spi (no need)
   // {//init
   //    printf("Initializing...\n"); 
   //    dwm_init();
   //    err_cnt += frst();
   //    printf("Done\n");
   // }
   
   /* ========= published APIs =========*/
   
   
   
   // setup tag update rate
   //ur_s_set = ur_set = 1;
   // HAL_Log("dwm_upd_rate_set(ur_set, ur_s_set);\n");
   //dwm_upd_rate_set(ur_set, ur_s_set);
   
   // setup PANID
   //panid = 1;
   // printf("dwm_panid_set(panid);\n");
   //dwm_panid_set(panid);
   
   // setup GPIO interrupt from "LOC_READY" event
   // HAL_Log("dwm_int_cfg_set(DWM1001_INTR_LOC_READY);\n");
   dwm_int_cfg_set(DWM1001_INTR_LOC_READY);
   // HAL_GPIO_SetupCb(HAL_GPIO_DRDY, HAL_GPIO_INT_EDGE_RISING, &gpio_cb);
   
   // setup encryption key for network
   // setup_enc();
   
   // while(1)
   // {
   	
      if(data_ready == 1)
      {
         data_ready = 0;
         get_loc();
      }
      nrf_delay_ms(1);
   // }
   
   // printf("err_cnt = %d \n", err_cnt);
      
   // Test_End();
}


int main(void) {
  // initialize RTT library
  ret_code_t error_code = NRF_SUCCESS;
  error_code = NRF_LOG_INIT(NULL);
  APP_ERROR_CHECK(error_code);
  NRF_LOG_DEFAULT_BACKENDS_INIT();
  printf("Log initialized!\n");

  // initialize interrupts
  gpio_init();
  // // Our own interrupt handler.
  // NRF_GPIOTE->CONFIG[0] = 1 | (RFM95_INT << 8) | (1 << 16);
  // NRF_GPIOTE->INTENSET = 1;
  // NVIC_EnableIRQ(GPIOTE_IRQn);


  spi_instance = &instance;

  nrf_drv_spi_config_t config = {
    .sck_pin = SPI_SCLK,
    .mosi_pin = SPI_MOSI,
    .miso_pin = SPI_MISO,
    .ss_pin = RFM95_CS,
    .irq_priority = NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
    .orc = 0,
    .frequency = NRF_DRV_SPI_FREQ_1M,
    .mode = NRF_DRV_SPI_MODE_0,
    .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
  };

  nrf_drv_spi_config_t dwm_config = {
    .sck_pin = DWM_SCLK,
    .mosi_pin = DWM_MOSI,
    .miso_pin = DWM_MISO,
    .ss_pin = DWM_CS,
    .irq_priority = NRFX_SPI_DEFAULT_CONFIG_IRQ_PRIORITY,
    .orc = 0,
    .frequency = NRF_DRV_SPI_FREQ_1M,
    .mode = NRF_DRV_SPI_MODE_0,
    .bit_order = NRF_DRV_SPI_BIT_ORDER_MSB_FIRST
  };

  spi_config = config;

  dwm_spi_config = dwm_config;


  nrf_gpio_pin_dir_set(RFM95_RST, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_pin_dir_set(DWM_RST, NRF_GPIO_PIN_DIR_OUTPUT);
  nrf_gpio_pin_write(RFM95_RST, 1);
  nrf_gpio_pin_write(DWM_RST, 1);
  printf("Arduino LoRa TX Test!\n");
  // manual reset
  nrf_gpio_pin_write(RFM95_RST, 0);
  nrf_gpio_pin_write(DWM_RST, 0);
  nrf_delay_ms(10);
  nrf_gpio_pin_write(RFM95_RST, 1);
  nrf_gpio_pin_write(DWM_RST, 1);
  nrf_delay_ms(10);


  while (!init()) {
    printf("LoRa radio init failed\n");
    while (1);
  }
  printf("LoRa radio init OK!\n");

  if (!setFrequency(RF95_FREQ)) {
    printf("setFrequency failed\n");
    while (1);
  }
  // Defaults after init are 434.0MHz, 13dBm, Bw = 125 kHz, Cr = 4/5, Sf = 128chips/symbol, CRC on
  // The default transmitter power is 13dBm, using PA_BOOST.
  // If you are using RFM95/96/97/98 modules which uses the PA_BOOST transmitter pin, then 
  // you can set transmitter powers from 5 to 23 dBm:
  setTxPower(23, false);

  while (1) {
    spi_les_test();
    if(flag == 1){
      loop_button_on();
    }
    if(flag == 2){
      loop_button_off();
    }
  }
}
