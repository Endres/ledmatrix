#include "w5100.h"
extern unsigned char					buf[];

void           Connect(unsigned char sock, unsigned char eth_protocol, unsigned char tcp_ip[4], unsigned int tcp_port);
unsigned char OpenSocket(unsigned char sock, unsigned char eth_protocol, unsigned int tcp_port);
void           CloseSocket(unsigned char sock);
void           DisconnectSocket(unsigned char sock);
unsigned char Listen(unsigned char sock);
void           UDPOpen(unsigned char sock, unsigned char udp_ip[4], unsigned int udp_port);
unsigned char Send(unsigned char sock, const unsigned char *buf, unsigned int buflen);
unsigned int  Receive(unsigned char sock, unsigned char *buf, unsigned int buflen);
unsigned int  ReceivedSize(unsigned char sock);
unsigned int  GetStatus(unsigned char sock);

void ethernet_setup(void);
//void ethernet_loop(void);
