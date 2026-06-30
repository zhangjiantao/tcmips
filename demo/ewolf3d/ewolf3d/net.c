#include "wl_def.h"
#ifdef LUMINARY
#include "luminary.h"

#define DHCPDISCOVER	1
#define DHCPOFFER	2
#define DHCPREQUEST	3
#define DHCPDECLINE	4
#define DHCPACK		5
#define DHCPNAK		6
#define DHCPRELEASE	7
#define DHCPINFORM	8

char dhcp_state = DHCPDISCOVER;
static uint32_t my_ip;
static uint32_t other_ip = 0xffffffff;
static uint8_t extra_opts[] = {54, 4, 0, 0, 0, 0, 50, 4, 0, 0, 0, 0, 0xff};

struct {
    uint32_t high;
    uint16_t low;
} my_mac;
#define mac_high my_mac.high
#define mac_low my_mac.low

static uint8_t raw_packetdata[352] __attribute((aligned(4)));

static const uint8_t packethdr[] = {
    /* Frame Length.  */
	0x50, 0x01,
    /* Ethernet header.  */
/* 0*/	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, /* Dest */
/* 6*/	0, 0, 0, 0, 0, 0, /* src */
/*12*/	0x08, 0x00, /* type (IP) */
    /* IP header.  */
/*14*/	0x45, 0x00, 0x01, 0x50, 0x00, 0x00,
/*20*/	0x40, 0x00, 0x40, 0x11, 0x39, 0x9e,
	0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff,
    /* UDP header.  */
/*34*/	0x00, 0x44, 0x00, 0x43,
/*38*/	0x01, 0x3c, 0, 0,
    /* BOOTP.  */
/*42*/  0x01, 0x01, 0x06, 0x00, 0xde, 0xad, 0xbe, 0xef
};

static const uint8_t dhcp_options[] = {
    /* Magic cookie.  */
    0x63, 0x82, 0x53, 0x63,
    /* DHCP message type.  */
    0x35, 0x01, 0x01,
    /* Maximum DHCP message size.  */
    0x39, 0x02, 0x02, 0x40,
    /* Parameter request list (subnet mask).  */
    0x37, 0x01, 0x01,
    /* End option.  */
    0xff
};

static void send_packet(void);

#define packetdata (raw_packetdata + 2)
#define destmac (packetdata + 0)
#define srcmac (packetdata + 6)
#define ip_packet (packetdata + 14)
#define ip_len (ip_packet + 2)
#define ip_csum (ip_packet + 10)
#define ip_src (ip_packet + 12)
#define ip_dest (ip_packet + 16)
#define udp_sport (ip_packet + 20)
#define udp_dport (ip_packet + 22)
#define udp_len (ip_packet + 24)
#define udp_csum (ip_packet + 26)
#define bootp_mac (ip_packet + 56)
#define dhcp_data (packetdata + 278)
#define opt_msg_type (dhcp_data + 6)
#define opt_end (dhcp_data + 14)

#if 0
#define NEXTB do { \
    if (v == 1) { \
	v = HWREG(ENET + 0x10); \
	if (length == 0) \
	    goto finish_packet; \
	length--; \
	c = v; \
	v = (v >> 8) | (1 << 24); \
    } else { \
	c = v; \
	v >>= 8; \
    } \
    } while (0)
#define SKIPB(n) do { \
    int skip_n = n; \
    while (skip_n && v != 1) { \
	v >>= 8; \
	skip_n--; \
    } \
    while (skip_n > 3 && length) { \
	HWREG(ENET + 0x10); \
	length--; \
	skip_n -= 4; \
	v = 1; \
    } \
    if (skip_n && length) { \
	v = (HWREG(ENET + 0x10) >> 8) | (1 << 24); \
	length--; \
	while (--skip_n) \
	    v >>= 8; \
    } \
    } while(0)
#else
#define NEXTB c = *(ip_data++)
#define SKIPB(n) ip_data += (n)
#endif

static uint8_t rx_packet[592] __attribute__((aligned(4)));

void dhcp_rx()
{
    uint32_t v;
    uint32_t new_ip;
    int length;
    uint8_t c;
    int send;
    int newstate;
    uint32_t *p;
    uint8_t *ip_data;

    send = 0;
    v = HWREG(ENET + 0x10);
    length = v & 0xffff;
    if (length < 288 || length > 592) {
	/* Wrong size.  Discard packet.  */
	length = (length - 1) >> 2;
	while (length--)
	    HWREG(ENET + 0x10);
	return;
    }
    p = (uint32_t *)rx_packet;
    *(p++) = v;
    length = (length - 1) >> 2;
    while (length) {
	*(p++) = HWREG(ENET + 0x10);
	length--;
    }
    /* IP Headers.  */
    ip_data = rx_packet + 16;
    /* Check xid.  */
    if (*(uint32_t *)(ip_data + 32) != 0xefbeadde)
	return;
    /* Check chaddr matches.  */
    if (memcmp(ip_data + 56, &my_mac, 6))
	return;
    new_ip = *(uint32_t *)(ip_data + 44);
    ip_data += 264;
    /* Option data.  */
    if (*(uint32_t *)ip_data != 0x63538263)
	return;
    ip_data += 4;
    newstate = dhcp_state;
    while (1) {
	NEXTB;
	if (c == 0xff)
	    break;
	switch (c) {
	case 53:
	    NEXTB; NEXTB;
	    switch (dhcp_state) {
	    case DHCPDISCOVER:
		if (c != DHCPOFFER)
		    return;
		newstate = DHCPREQUEST;
		break;
	    case DHCPREQUEST:
		if (c == DHCPACK) {
		    newstate = DHCPINFORM;
		} else if (c == DHCPNAK) {
		    /* FIXME: Pick a link local address??? */
		    newstate = DHCPINFORM;
		} else {
		    return;
		}
		break;
	    default:
		return;
	    }
	    break;
	case 54:
	    NEXTB; NEXTB;
	    extra_opts[2] = c;
	    NEXTB;
	    extra_opts[3] = c;
	    NEXTB;
	    extra_opts[4] = c;
	    NEXTB;
	    extra_opts[5] = c;
	    break;
	default:
	    NEXTB;
	    SKIPB(c);
	    break;
	}
    }
    if (newstate == dhcp_state)
	return;

    dhcp_state = newstate;
    *opt_msg_type = dhcp_state;
    memcpy(extra_opts + 8, &new_ip, 4);
    memcpy(opt_end, extra_opts, sizeof(extra_opts));

    if (dhcp_state == DHCPINFORM)
	my_ip = new_ip;
    else
	send_packet();
}

void ethernet_isr(void)
{
  /* ACK all interrupts.  */
  HWREG(ENET + 0x00) = HWREG(ENET + 0x00);

  while (HWREG(ENET + 0x34)) {
      /* Recieve packet.  */
      if (dhcp_state != DHCPACK) {
	  dhcp_rx();
      } else {
	  uint32_t v;
	  int length;

	  v = HWREG(ENET + 0x10);
	  length = ((v & 0xffff) - 1) >> 2;
	  while (length--)
	      HWREG(ENET + 0x10);
      }
  }

}

static void do_ip_csum(void)
{
  return;
    uint16_t *p = (uint16_t *)ip_packet;
    uint32_t csum;
    int i;

    ip_csum[0] = ip_csum[1] = 0;
    csum = ~(uint16_t)0;
    for (i = 0; i < (336 / 2); i++) {
	csum += *(p++);
    }
    while (csum >> 16)
	csum = (csum & 0xffff) + (csum >> 16);
    ip_csum[0] = csum;
    ip_csum[1] = csum >> 8;
}

static void send_packet(void)
{
    uint32_t *p;
    int n;

    *(uint32_t *)ip_src = my_ip;
    *(uint32_t *)ip_dest = other_ip;
    do_ip_csum();
    p = (uint32_t *)raw_packetdata;
    for (n = 0; n < sizeof(raw_packetdata) / 4; n++) {
	HWREG(ENET + 0x10) = *(p++);
    }
    HWREG(ENET + 0x38) = 1;
}

void net_init()
{
    uint32_t mac0;
    uint32_t mac1;

    /* Setup packet data.  */
    memcpy(raw_packetdata, packethdr, sizeof(packethdr));
    memcpy(dhcp_data, dhcp_options, sizeof(dhcp_options));
    mac0 = HWREG(SYSCTL + 0x1e0);
    mac1 = HWREG(SYSCTL + 0x1e4);
    mac_high = mac0 | (mac1 << 24);
    mac_low = mac1 >> 8;
    memcpy(srcmac, &my_mac, 6);
    memcpy(bootp_mac, &my_mac, 6);

    /* Enable ethernet interrupts.  */
    NVIC[0x41] |= (1 << (42 - 32));

    /* Initialize ethernet controller.  */
    /* Enable MAC+PHY peripheral clocks.  */
    HWREG(SYSCTL + 0x108) |= (1 << 28) | (1 << 30);
    /* Disable all interrupts.  */
    HWREG(ENET + 0x04) = 0;
    /* Set MAC address.  */
    HWREG(ENET + 0x14) = mac_high;
    HWREG(ENET + 0x18) = mac_low;
    HWREG(ENET + 0x08) = 3; /* RX enable, AMUL.  */
    HWREG(ENET + 0x0c) = 0x17; /* TX enable, DUPLEX, CRC, PAD.  */

    /* Enable RX interrupts.  */
    HWREG(ENET + 0x04) |= 1;

    send_packet();
}
#endif


