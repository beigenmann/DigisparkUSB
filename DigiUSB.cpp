/* Name: DigiUSB.c
 * Based on V-USB Arduino Examples by Philip J. Lindsay
 * Modification for the Digispark by Erik Kettenburg, Digistump LLC
 * VID/PID changed to pair owned by Digistump LLC, code modified to use
 * pinchange int for attiny85
 * Original notice below:
 * Based on project: hid-data, example how to use HID for data transfer
 * (Uses modified descriptor and usbFunctionSetup from it.)
 * Original author: Christian Starkjohann
 * Arduino modifications by: Philip J. Lindsay
 * Creation Date: 2008-04-11
 * Tabsize: 4
 * Copyright: (c) 2008 by OBJECTIVE DEVELOPMENT Software GmbH
 * License: GNU GPL v2 (see License.txt), GNU GPL v3 or proprietary
 * (CommercialLicense.txt)
 * This Revision: $Id: main.c 692 2008-11-07 15:07:40Z cs $
 */

/*
This example should run on most AVRs with only little changes. No special
hardware resources except INT0 are used. You may have to change usbconfig.h for
different I/O pins for USB. Please note that USB D+ must be the INT0 pin, or
at least be connected to INT0 as well.
*/

#include <Arduino.h>
#include <avr/eeprom.h>
#include <avr/interrupt.h> /* for sei() */
#include <avr/io.h>
#include <avr/pgmspace.h> /* required by usbdrv.h */
#include <avr/wdt.h>
#include <util/delay.h> /* for _delay_ms() */

#ifdef __cplusplus
extern "C" {
#endif
#include "usbdrv.h"
#ifdef __cplusplus
} // extern "C"
#endif
#include "oddebug.h" /* This is also an example for using debug macros */

#include "DigiUSB.h"
uchar debug_s = 0;
uchar pluggedInterface = 1;
// Ring buffer implementation nicked from HardwareSerial.cpp
// TODO: Don't nick it. :)
ring_buffer rx_buffer = {{0}, 0, 0};
ring_buffer tx_buffer = {{0}, 0, 0};

inline int store_char(unsigned char c, ring_buffer *the_buffer) {
  int i = (the_buffer->head + 1) % RING_BUFFER_SIZE;

  // if we should be storing the received character into the location
  // just before the tail (meaning that the head would advance to the
  // current location of the tail), we're about to overflow the buffer
  // and so we don't write the character or advance the head.
  if (i != the_buffer->tail) {
    the_buffer->buffer[the_buffer->head] = c;
    the_buffer->head = i;
    return 1;
  }
  return 0;
}

DigiUSBDevice::DigiUSBDevice(ring_buffer *rx_buffer, ring_buffer *tx_buffer) {
  _rx_buffer = rx_buffer;
  _tx_buffer = tx_buffer;
}
uchar DigiUSBDevice::debug() { return debug_s; }
void DigiUSBDevice::begin() {
  cli();

  usbInit();

  usbDeviceDisconnect();
  uchar i;
  i = 0;
  while (--i) { /* fake USB disconnect for > 250 ms */
    _delay_ms(10);
  }
  usbDeviceConnect();

  sei();
}

// TODO: Deprecate update
void DigiUSBDevice::update() { refresh(); }

void DigiUSBDevice::refresh() { usbPoll(); }

// wait a specified number of milliseconds (roughly), refreshing in the
// background
void DigiUSBDevice::delay(long milli) {
  unsigned long last = millis();
  while (milli > 0) {
    unsigned long now = millis();
    milli -= now - last;
    last = now;
    refresh();
  }
}

int DigiUSBDevice::available() {
  /*
   */
  return (RING_BUFFER_SIZE + _rx_buffer->head - _rx_buffer->tail) %
         RING_BUFFER_SIZE;
}

int DigiUSBDevice::tx_remaining() {
  return RING_BUFFER_SIZE -
         (RING_BUFFER_SIZE + _tx_buffer->head - _tx_buffer->tail) %
             RING_BUFFER_SIZE;
}

int DigiUSBDevice::read() {
  /*
   */
  // if the head isn't ahead of the tail, we don't have any characters
  if (_rx_buffer->head == _rx_buffer->tail) {
    return -1;
  } else {
    unsigned char c = _rx_buffer->buffer[_rx_buffer->tail];
    _rx_buffer->tail = (_rx_buffer->tail + 1) % RING_BUFFER_SIZE;
    return c;
  }
}

size_t DigiUSBDevice::write(byte c) {
  /*
   */
  return store_char(c, _tx_buffer);
}

// TODO: Handle this better?
int tx_available() {
  /*
   */
  return (RING_BUFFER_SIZE + tx_buffer.head - tx_buffer.tail) %
         RING_BUFFER_SIZE;
}

int tx_read() {
  /*
   */
  // if the head isn't ahead of the tail, we don't have any characters
  if (tx_buffer.head == tx_buffer.tail) {
    return -1;
  } else {
    unsigned char c = tx_buffer.buffer[tx_buffer.tail];
    tx_buffer.tail = (tx_buffer.tail + 1) % RING_BUFFER_SIZE;
    return c;
  }
}

/* ------------------------------------------------------------------------- */
/* ----------------------------- USB interface ----------------------------- */
/* ------------------------------------------------------------------------- */

#ifdef __cplusplus
extern "C" {
#endif
const uint8_t BOS_DESCRIPTOR_PREFIX[] PROGMEM = {
    // BOS descriptor header
    0x05, 0x0F, 0x39, 0x00, 0x02,

    // WebUSB Platform Capability descriptor
    0x18, // Descriptor size (24 bytes)
    0x10, // Descriptor type (Device Capability)
    0x05, // Capability type (Platform)
    0x00, // Reserved

    // WebUSB Platform Capability ID (3408b638-09a9-47a0-8bfd-a0768815b665)
    0x38, 0xB6, 0x08, 0x34, 0xA9, 0x09, 0xA0, 0x47, 0x8B, 0xFD, 0xA0, 0x76,
    0x88, 0x15, 0xB6, 0x65,

    0x00, 0x01,        // WebUSB version 1.0
    WL_REQUEST_WEBUSB, // Vendor-assigned WebUSB request code
};
// Landing page (1 byte) sent in the middle.

const uint8_t BOS_DESCRIPTOR_SUFFIX[] PROGMEM{
    // Microsoft OS 2.0 Platform Capability Descriptor
    // Thanks http://janaxelson.com/files/ms_os_20_descriptors.c
    0x1C, // Descriptor size (28 bytes)
    0x10, // Descriptor type (Device Capability)
    0x05, // Capability type (Platform)
    0x00, // Reserved

    // MS OS 2.0 Platform Capability ID (D8DD60DF-4589-4CC7-9CD2-659D9E648A9F)
    0xDF, 0x60, 0xDD, 0xD8, 0x89, 0x45, 0xC7, 0x4C, 0x9C, 0xD2, 0x65, 0x9D,
    0x9E, 0x64, 0x8A, 0x9F,

    0x00, 0x00, 0x03, 0x06, // Windows version (8.1) (0x06030000)
    MS_OS_20_DESCRIPTOR_LENGTH, 0x00,
    WL_REQUEST_WINUSB, // Vendor-assigned bMS_VendorCode
    0x00               // Doesn’t support alternate enumeration
};
const uint8_t MS_OS_20_DESCRIPTOR_PREFIX[] PROGMEM = {
    // Microsoft OS 2.0 descriptor set header (table 10)
    0x0A, 0x00,             // Descriptor size (10 bytes)
    0x00, 0x00,             // MS OS 2.0 descriptor set header
    0x00, 0x00, 0x03, 0x06, // Windows version (8.1) (0x06030000)
    0x2e, 0x00,             // Size, MS OS 2.0 descriptor set

    // Microsoft OS 2.0 configuration subset header
    0x08, 0x00, // Descriptor size (8 bytes)
    0x01, 0x00, // MS OS 2.0 configuration subset header
    0x00,       // bConfigurationValue
    0x00,       // Reserved
    0x24, 0x00, // Size, MS OS 2.0 configuration subset

    // Microsoft OS 2.0 function subset header
    0x08, 0x00, // Descriptor size (8 bytes)
    0x02, 0x00, // MS OS 2.0 function subset header
};

// First interface number (1 byte) sent here.

const uint8_t MS_OS_20_DESCRIPTOR_SUFFIX[] PROGMEM = {
    0x00,       // Reserved
    0x1c, 0x00, // Size, MS OS 2.0 function subset

    // Microsoft OS 2.0 compatible ID descriptor (table 13)
    0x14, 0x00, // wLength
    0x03, 0x00, // MS_OS_20_FEATURE_COMPATIBLE_ID
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

const uchar MS_OS_20_DESCRIPTOR_SET[MS_OS_20_DESCRIPTOR_LENGTH] PROGMEM = {
    // Microsoft OS 2.0 descriptor set header (table 10)
    0x0A, 0x00,                       // Descriptor size (10 bytes)
    0x00, 0x00,                       // MS OS 2.0 descriptor set header
    0x00, 0x00, 0x03, 0x06,           // Windows version (8.1) (0x06030000)
    MS_OS_20_DESCRIPTOR_LENGTH, 0x00, // Size, MS OS 2.0 descriptor set

    // Microsoft OS 2.0 compatible ID descriptor (table 13)
    0x14, 0x00, // wLength
    0x03, 0x00, // MS_OS_20_FEATURE_COMPATIBLE_ID
    'W', 'I', 'N', 'U', 'S', 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00,
};

PROGMEM const char usbDescriptorConfiguration[] = {
    /* USB configuration descriptor */
    9, /* sizeof(usbDescriptorConfiguration): length of descriptor in bytes */
    USBDESCR_CONFIG, /* descriptor type */
    18 + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT + 7 * USB_CFG_HAVE_INTRIN_ENDPOINT3 +
        (USB_CFG_DESCR_PROPS_HID & 0xff),
    0,
    /* total length of data returned (including inlined descriptors) */
    1, /* number of interfaces in this configuration */
    1, /* index of this configuration */
    0, /* configuration name string index */
#if USB_CFG_IS_SELF_POWERED
    (1 << 7) | USBATTR_SELFPOWER, /* attributes */
#else
    (1 << 7), /* attributes */
#endif
    USB_CFG_MAX_BUS_POWER / 2, /* max USB current in 2mA units */
                               /* interface descriptor follows inline: */
    9, /* sizeof(usbDescrInterface): length of descriptor in bytes */
    USBDESCR_INTERFACE, /* descriptor type */
    0,                  /* index of this interface */
    0,                  /* alternate setting for this interface */
    USB_CFG_HAVE_INTRIN_ENDPOINT +
        USB_CFG_HAVE_INTRIN_ENDPOINT3, /* endpoints excl 0: number of endpoint
                                          descriptors to follow */
    USB_CFG_INTERFACE_CLASS, USB_CFG_INTERFACE_SUBCLASS,
    USB_CFG_INTERFACE_PROTOCOL, 0,   /* string index for interface */
#if (USB_CFG_DESCR_PROPS_HID & 0xff) /* HID descriptor */
    9,            /* sizeof(usbDescrHID): length of descriptor in bytes */
    USBDESCR_HID, /* descriptor type: HID */
    0x01, 0x01,   /* BCD representation of HID version */
    0x00,         /* target country code */
    0x01, /* number of HID Report (or other HID class) Descriptor infos to
             follow */
    0x22, /* descriptor type: report */
    USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH,
    0, /* total length of report descriptor */
#endif
#if USB_CFG_HAVE_INTRIN_ENDPOINT /* endpoint descriptor for endpoint 1 */
    7,                           /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,           /* descriptor type = endpoint */
    (char)0x81,                  /* IN endpoint number 1 */
    0x03,                        /* attrib: Interrupt endpoint */
    8, 0,                        /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL,  /* in ms */
#endif
#if USB_CFG_HAVE_INTRIN_ENDPOINT3 /* endpoint descriptor for endpoint 3 */
    7,                            /* sizeof(usbDescrEndpoint) */
    USBDESCR_ENDPOINT,            /* descriptor type = endpoint */
    (char)0x83,                   /* IN endpoint number 1 */
    0x03,                         /* attrib: Interrupt endpoint */
    8, 0,                         /* maximum packet size */
    USB_CFG_INTR_POLL_INTERVAL,   /* in ms */
#endif
};

PROGMEM const char usbHidReportDescriptor[22] = {
    /* USB report descriptor */
    0x06, 0x00, 0xff, // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,       // USAGE (Vendor Usage 1)
    0xa1, 0x01,       // COLLECTION (Application)
    0x15, 0x00,       //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00, //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,       //   REPORT_SIZE (8)
    0x95, 0x01,       //   REPORT_COUNT (1)
    0x09, 0x00,       //   USAGE (Undefined)
    0xb2, 0x02, 0x01, //   FEATURE (Data,Var,Abs,Buf)
    0xc0              // END_COLLECTION
};

/* Since we define only one feature report, we don't use report-IDs (which
 * would be the first byte of the report). The entire report consists of 1
 * opaque data bytes.
 */

/* ------------------------------------------------------------------------- */
static uchar buffer[64];
// const uchar pluggedInterface = 1;
// const uchar *pmResponsePtr = NULL;
// uchar pmResponseBytesRemaining = 0;
// const char serial[] PROGMEM = "Und noch ein langer zweiter Satz.";
uchar usbFunctionDescriptor(usbRequest_t *rq) {
  switch (rq->wValue.bytes[1]) {
  case USB_BOS_DESCRIPTOR_TYPE: {
    int length = sizeof(BOS_DESCRIPTOR_PREFIX);
    memcpy_P(buffer, &BOS_DESCRIPTOR_PREFIX, length);
    memcpy(&buffer[length], &numUrls, 1);
    length++;
    memcpy_P(&buffer[length], &BOS_DESCRIPTOR_SUFFIX,
             sizeof(BOS_DESCRIPTOR_SUFFIX));
    length += sizeof(BOS_DESCRIPTOR_SUFFIX);
    usbMsgPtr = (uchar *)(buffer);
    return length;
  } break;
  case USBDESCR_CONFIG:
    usbMsgPtr = (uchar *)usbDescriptorConfiguration;
    return sizeof(usbDescriptorConfiguration);
  }
}

int _i = 0;
usbMsgLen_t usbFunctionSetup(uchar data[8]) {
  usbRequest_t *rq = (usbRequest_t *)((void *)data);
  if ((rq->bmRequestType & USBRQ_TYPE_MASK) ==
      USBRQ_TYPE_CLASS) {                       /* HID class request */
    if (rq->bRequest == USBRQ_HID_GET_REPORT) { /* wValue: ReportType
                                                   (highbyte), ReportID
                                                   (lowbyte) */
      /* since we have only one report type, we can ignore the report-ID */
      static uchar dataBuffer[1]; /* buffer must stay valid when
                                      returns */
      if (tx_available()) {
        dataBuffer[0] = tx_read();
        usbMsgPtr = dataBuffer; /* tell the driver which data to return */
        return 1;               /* tell the driver to send 1 byte */
      } else {
        // Drop through to return 0 (which will stall the request?)
      }
    } else if (rq->bRequest == USBRQ_HID_SET_REPORT) {
      /* since we have only one report type, we can ignore the report-ID */

      // TODO: Check race issues?
      store_char(rq->wIndex.bytes[0], &rx_buffer);
    }
  } else {
    /* ignore vendor type requests, we don't use any */
  }

  switch (rq->bRequest) {
  case WL_REQUEST_WEBUSB:
    switch (rq->wIndex.word) {
    case WEBUSB_REQUEST_GET_ALLOWED_ORIGINS:

      break;
    case WEBUSB_REQUEST_GET_URL: {
      _i++;
      debug_s = _i;
      const WebUSBURL &url = URLS[rq->wValue.bytes[0] - 1];
      int urlLength = strlen(url.url);
      int descriptorLength = urlLength + 3;
      buffer[0] = descriptorLength;
      buffer[1] = 3;
      buffer[2] = url.scheme;

      memcpy(buffer + 3, url.url, urlLength);
      // pmResponsePtr = buffer;
      // pmResponseBytesRemaining = descriptorLength;
      usbMsgPtr = (uchar *)(buffer);
      return descriptorLength;
    }
    }
  case WL_REQUEST_WINUSB: {

#if 1
    int length = sizeof(MS_OS_20_DESCRIPTOR_SET);
    memcpy_P(buffer, &MS_OS_20_DESCRIPTOR_SET, length);
    usbMsgPtr = (uchar *)(buffer);
    return length;
#endif
#if 0
    int length = sizeof(MS_OS_20_DESCRIPTOR_PREFIX);
    memcpy_P(buffer, &MS_OS_20_DESCRIPTOR_PREFIX, length);
    buffer[length] = pluggedInterface;
    length++;
    memcpy_P(&buffer[length], &MS_OS_20_DESCRIPTOR_SUFFIX,
             sizeof(MS_OS_20_DESCRIPTOR_SUFFIX));
    length += sizeof(MS_OS_20_DESCRIPTOR_SUFFIX);
    usbMsgPtr = (uchar *)(buffer);
    return length;
#endif
  }
  }

  return 0;
}
uchar usbFunctionRead(uchar *data, uchar len) {

  /*if (pmResponseBytesRemaining == 0)
    return 0;

  if (len > pmResponseBytesRemaining) {
    len = pmResponseBytesRemaining;
  }
  memcpy(data, pmResponsePtr, len);
  pmResponsePtr += len;
  pmResponseBytesRemaining -= len;
  return len;*/
  return 0;
}

/*---------------------------------------------------------------------------*/
/* usbFunctionWrite                                                          */
/*---------------------------------------------------------------------------*/
uchar usbFunctionWrite(uchar *data, uchar len) { return 0; }
#ifdef __cplusplus
} // extern "C"
#endif

DigiUSBDevice DigiUSB = DigiUSBDevice(&rx_buffer, &tx_buffer);
