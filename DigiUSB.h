/*
 * Based on Obdev's AVRUSB code and under the same license.
 *
 * TODO: Make a proper file header. :-)
 */
#ifndef __DigiUSB_h__
#define __DigiUSB_h__

#include "Print.h"
#include "usbdrv.h"
#include <avr/interrupt.h>
#include <avr/pgmspace.h>
#include <string.h>

typedef uint8_t byte;

#include <util/delay.h> /* for _delay_ms() */

#define RING_BUFFER_SIZE 48

#define USB_BOS_DESCRIPTOR_TYPE 15
#define WL_REQUEST_WINUSB (252)
#define WL_REQUEST_WEBUSB (254)

#define WEBUSB_REQUEST_GET_ALLOWED_ORIGINS 0x01
#define WEBUSB_REQUEST_GET_URL 0x02
#define REQUEST_TYPE 0x60
#define REQUEST_VENDOR 0x40
#define MS_OS_20_REQUEST_DESCRIPTOR 0x07
#define MS_OS_20_DESCRIPTOR_LENGTH (0x1e)
#define MS_OS_20_REQUEST_DESCRIPTOR 0x07
struct ring_buffer {
  unsigned char buffer[RING_BUFFER_SIZE];
  int head;
  int tail;
};

typedef struct {
  uint8_t scheme;
  const char *url;
} WebUSBURL;


extern const WebUSBURL URLS[] ;
extern const uint8_t numUrls;

class DigiUSBDevice : public Print {
private:
  ring_buffer *_rx_buffer;
  ring_buffer *_tx_buffer;

public:
  DigiUSBDevice(ring_buffer *rx_buffer, ring_buffer *tx_buffer);
  uchar debug();
  void begin();

  // TODO: Deprecate update
  void update();

  void refresh();

  void delay(long milliseconds);

  int available();
  int tx_remaining();

  int read();
  virtual size_t write(byte c);
  using Print::write;
};

extern DigiUSBDevice DigiUSB;

#endif // __DigiUSB_h__
