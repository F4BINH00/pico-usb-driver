#include "tusb.h"
#include "pico/unique_id.h"

#define USB_VID 0xCAFE
#define USB_PID 0x4001
#define USB_BCD 0x0100

// Endpoint numbers
#define EPNUM_VENDOR_OUT   0x01
#define EPNUM_VENDOR_IN    0x81

//------------- Device Descriptor -------------//
tusb_desc_device_t const desc_device =
{
    .bLength            = sizeof(tusb_desc_device_t),
    .bDescriptorType    = TUSB_DESC_DEVICE,
    .bcdUSB             = 0x0200,

    .bDeviceClass       = TUSB_CLASS_VENDOR_SPECIFIC,
    .bDeviceSubClass    = 0x00,
    .bDeviceProtocol    = 0x00,

    .bMaxPacketSize0    = CFG_TUD_ENDPOINT0_SIZE,

    .idVendor           = USB_VID,
    .idProduct          = USB_PID,
    .bcdDevice          = USB_BCD,

    .iManufacturer      = 0x01,
    .iProduct           = 0x02,
    .iSerialNumber      = 0x03,

    .bNumConfigurations = 0x01
};

uint8_t const * tud_descriptor_device_cb(void)
{
  return (uint8_t const *) &desc_device;
}

//------------- Configuration Descriptor -------------//
enum {
  ITF_NUM_VENDOR = 0,
  ITF_NUM_TOTAL
};

#define CONFIG_TOTAL_LEN  (TUD_CONFIG_DESC_LEN + TUD_VENDOR_DESC_LEN)

uint8_t const desc_configuration[] =
{
  TUD_CONFIG_DESCRIPTOR(1, ITF_NUM_TOTAL, 0, CONFIG_TOTAL_LEN, 0x00, 100),

  // Vendor interface with 2 bulk endpoints
  TUD_VENDOR_DESCRIPTOR(ITF_NUM_VENDOR, 4, EPNUM_VENDOR_OUT, EPNUM_VENDOR_IN, 64)
};

uint8_t const* tud_descriptor_configuration_cb(uint8_t index)
{
  (void) index;
  return desc_configuration;
}

//------------- String Descriptors -------------//
static char serial_str[2*PICO_UNIQUE_BOARD_ID_SIZE_BYTES + 1];

static uint16_t const desc_string0[] = { 0x0409 }; 

static char const* string_desc_arr[] =
{
  (const char[]) { 0x09, 0x04 }, 
  "UFC - Sistemas Embarcados",   
  "pico_usb generic device",     
  serial_str,                    
  "Vendor Interface",            
};

static uint16_t _desc_str[64];

uint16_t const* tud_descriptor_string_cb(uint8_t index, uint16_t langid)
{
  (void) langid;

  uint8_t chr_count;

  if (index == 0)
  {
    memcpy(&_desc_str[1], desc_string0, 2);
    chr_count = 1;
  }
  else
  {
    if (index == 3)
    {
      // Build a serial from Pico unique id
      pico_unique_board_id_t id;
      pico_get_unique_board_id(&id);
      for (int i = 0; i < PICO_UNIQUE_BOARD_ID_SIZE_BYTES; i++) {
        sprintf(&serial_str[2*i], "%02X", id.id[i]);
      }
      serial_str[2*PICO_UNIQUE_BOARD_ID_SIZE_BYTES] = 0;
    }

    if (index >= sizeof(string_desc_arr)/sizeof(string_desc_arr[0])) return NULL;

    const char* str = string_desc_arr[index];
    chr_count = (uint8_t) strlen(str);

    if (chr_count > 63) chr_count = 63;

    for(uint8_t i=0; i<chr_count; i++)
    {
      _desc_str[1+i] = str[i];
    }
  }

  _desc_str[0] = (TUSB_DESC_STRING << 8) | (2*chr_count + 2);

  return _desc_str;
}
