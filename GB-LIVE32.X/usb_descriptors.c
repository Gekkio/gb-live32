#include "usb.h"
#include "usb_device_cdc.h"

#define USB_DESCRIPTOR_INTERFACE_ASSOCIATION 11

typedef struct __attribute__ ((packed)) _USB_INTERFACE_ASSOCIATION_DESCRIPTOR {
  uint8_t bLength;
  uint8_t bDescriptorType;
  uint8_t bFirstInterface;
  uint8_t bInterfaceCount;
  uint8_t bFunctionClass;
  uint8_t bFunctionSubClass;
  uint8_t bFunctionProtocol;
  uint8_t iFunction;
} USB_INTERFACE_ASSOCIATION_DESCRIPTOR;

const USB_DEVICE_DESCRIPTOR device_dsc = {
  sizeof(device_dsc), // bLength
  USB_DESCRIPTOR_DEVICE, // bDescriptorType
  0x0200, // 0x0200 = USB 2.0, 0x0110 = USB 1.1
  0xfe, // Device class
  0x02,
  0x01,
  USB_EP0_BUFF_SIZE, // bMaxPacketSize0
  0x16C0, // Vendor
  0x05E1, // Product
  0x0002, // device release (2.0)
  1, // Manufacturer
  2, // Product
  0, // Serial
  1
};

struct Configuration1 {
  USB_CONFIGURATION_DESCRIPTOR config;
  USB_INTERFACE_ASSOCIATION_DESCRIPTOR cdc_interface_assoc;
  USB_INTERFACE_DESCRIPTOR cdc_interface;
  USB_CDC_HEADER_FN_DSC cdc_header;
  USB_CDC_ACM_FN_DSC cdc_acm;
  USB_CDC_UNION_FN_DSC cdc_union;

  USB_ENDPOINT_DESCRIPTOR cdc_notification;
  USB_INTERFACE_DESCRIPTOR cdc_data_interface;
  USB_ENDPOINT_DESCRIPTOR cdc_data_out;
  USB_ENDPOINT_DESCRIPTOR cdc_data_in;
};

static const struct Configuration1 configuration_1 = {
  {
    sizeof(USB_CONFIGURATION_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_CONFIGURATION, // bDescriptorType
    sizeof(struct Configuration1), // wTotalLength
    2, // bNumInterfaces
    1, // bConfigurationValue
    0, // iConfiguration
    USB_CFG_DSC_REQUIRED, // bmAttributes
    30 / 2, // bMaxPower
  },
  {
    sizeof(USB_INTERFACE_ASSOCIATION_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_INTERFACE_ASSOCIATION, // bDescriptorType
    0, // bFirstInterface
    2, // bInterfaceCount
    COMM_INTF, // bFunctionClass
    ABSTRACT_CONTROL_MODEL, // bFunctionSubClass
    NO_PROTOCOL, // bInterfaceProtocol
    0, // iFunction
  },
  {
    sizeof(USB_INTERFACE_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_INTERFACE, // bDescriptorType
    0, // bInterfaceNumber
    0, // bAlternateSetting
    1, // bNumEndpoints
    COMM_INTF, // bInterfaceClass
    ABSTRACT_CONTROL_MODEL, // bInterfaceSubClass
    NO_PROTOCOL, // bInterfaceProtocol
    0, // iInterface
  },
  {
    sizeof(USB_CDC_HEADER_FN_DSC), // bFNLength
    CS_INTERFACE, // bDscType
    DSC_FN_HEADER, // bDscSubType
    0x0110, // bcdCDC
  },
  {
    sizeof(USB_CDC_ACM_FN_DSC), // bFNLength
    CS_INTERFACE, // bDscType
    DSC_FN_ACM, // bDscSubType
    USB_CDC_ACM_FN_DSC_VAL, // bmCapabilities
  },
  {
    sizeof(USB_CDC_UNION_FN_DSC), // bFNLength
    CS_INTERFACE, // bDscType
    DSC_FN_UNION, // bDscSubType
    CDC_COMM_INTF_ID, // bMasterIntf
    CDC_DATA_INTF_ID, // bSlaveIntf0
  },
  {
    sizeof(USB_ENDPOINT_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_ENDPOINT, // bDescriptorType
    _EP01_IN, // bEndpointAddress
    EP_ATTR_INTR, // bmAttributes
    CDC_COMM_IN_EP_SIZE, // wMaxPacketSize
    0x01, // bInterval
  },
  {
    sizeof(USB_INTERFACE_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_INTERFACE, // bDescriptorType
    1, // bInterfaceNumber
    0, // bAlternateSetting
    2, // bNumEndpoints
    DATA_INTF, // bInterfaceClass
    0, // bInterfaceSubClass
    NO_PROTOCOL, // bInterfaceProtocol
    0, // iInterface
  },
  {
    sizeof(USB_ENDPOINT_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_ENDPOINT, // bDescriptorType
    _EP02_IN, // bEndpointAddress
    _BULK, // bmAttributes
    CDC_DATA_IN_EP_SIZE, // wMaxPacketSize
    1, // bInterval
  },
  {
    sizeof(USB_ENDPOINT_DESCRIPTOR), // bLength
    USB_DESCRIPTOR_ENDPOINT, // bDescriptorType
    _EP02_OUT, // bEndpointAddress
    _BULK, // bmAttributes
    CDC_DATA_OUT_EP_SIZE, // wMaxPacketSize
    1, // bInterval
  },
};

const uint8_t *const USB_CD_Ptr[] = {
  (const uint8_t *) &configuration_1,
};

#define STRING(L) const struct {uint8_t bLength; uint8_t bDescriptorType; uint16_t bString[L];}

static STRING(1) string_0 = {
  sizeof(string_0),
  USB_DESCRIPTOR_STRING,
  {0x0409}
};

static STRING(9) string_manufacturer = {
  sizeof(string_manufacturer),
  USB_DESCRIPTOR_STRING,
  {'g', 'e', 'k', 'k', 'i', 'o', '.', 'f', 'i',}
};

static STRING(9) string_product = {
  sizeof(string_product),
  USB_DESCRIPTOR_STRING,
  {'G', 'B', '-', 'L', 'I', 'V', 'E', '3', '2',}
};

const uint8_t *const USB_SD_Ptr[USB_NUM_STRING_DESCRIPTORS] = {
  (const uint8_t *) &string_0,
  (const uint8_t *) &string_manufacturer,
  (const uint8_t *) &string_product,
};
