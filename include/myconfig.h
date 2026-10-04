//
//
#ifndef MYCONFIG_H
#define MYCONFIG_H

////// DHCP/Stacic IP selection
// #define USE_DHCP
#define USE_STATIC_IP       // Comment out this line if you use DHCP

////// Settings of this device
#define MY_IPADDRESS    "192.168.0.30"

////// Reconnect retry max
#define RECONECT_RETRY_MAX  30  // 2.5 minutes considering router reboot

////// Watchdog timer timeout
// Longer value considering router reboot time
#define WDT_TIMEOUT_SECONDS 180

#endif