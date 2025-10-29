#ifndef _LWIPOPTS_H
#define _LWIPOPTS_H

#include "lwipopts_examples_common.h"

/* TCP WND must be at least 16 kb to match TLS record size
   or you will get a warning "altcp_tls: TCP_WND is smaller than the RX decrypion buffer, connection RX might stall!" */
#undef TCP_WND
#define TCP_WND  16384

#define LWIP_ALTCP           1
#define LWIP_DEBUG           0  /* TGE */
#define LWIP_STATS           0
#define LWIP_STATS_DISPLAY   0

// Two different modes:
#define PICO_CYW43_ARCH_THREADSAFE_BACKGROUND 1
#undef PICO_CYW43_ARCH_POLL

// TGE:
/*
extern void tge_print_lwip(const char *format, ...);
#define LWIP_PLATFORM_DIAG(msg) do { tge_print_lwip msg; } while(0)

extern void tge_flush_lwip();
#define LWIP_PLATFORM_ASSERT(x) do { tge_print_lwip("Assertion \"%s\" failed at line %d in %s\n", x, __LINE__, __FILE__); tge_flush_lwip(); abort(); } while(0)
*/

#endif

