#include <avr/io.h>
#include <string.h>
#include <stdio.h>
#ifndef F_CPU
#define F_CPU 8000000UL
#endif
#include <util/delay.h>
#include <avr/pgmspace.h>
#include "ethernet.h"


#define  MAX_BUF					512			/* largest buffer we can read from chip */

unsigned char						buf[MAX_BUF];

void Connect(unsigned char  sock, unsigned char eth_protocol, unsigned char tcp_ip[4], unsigned int tcp_port)
{
	unsigned int			sockaddr;
    if (sock >= W5100_NUM_SOCKETS)  return;	// illegal socket value is bad!

	sockaddr =  W5100_SKT_BASE(sock);				// calc base addr for this socket

    if (W51_read(sockaddr+W5100_SR_OFFSET) == W5100_SKT_SR_CLOSED)    // Make sure we close the socket first
	{
		CloseSocket(sock);
    }

    W51_write(sockaddr+W5100_MR_OFFSET ,eth_protocol);		// set protocol for this socket
    W51_write(sockaddr+W5100_DIPR_OFFSET, tcp_ip[0]);		// set destination ip for this socket
    W51_write(sockaddr+W5100_DIPR_OFFSET + 1, tcp_ip[1]);
    W51_write(sockaddr+W5100_DIPR_OFFSET + 2, tcp_ip[2]);
    W51_write(sockaddr+W5100_DIPR_OFFSET + 3, tcp_ip[3]);
    W51_write(sockaddr+W5100_DPORT_OFFSET, ((tcp_port & 0xFF00) >> 8 ));		// set destination port for this socket (MSB)
    W51_write(sockaddr+W5100_DPORT_OFFSET + 1, (tcp_port & 0x00FF));			// set destination port for this socket (LSB)
    W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_CONNECT);	               	// open the socket

    while (W51_read(sockaddr+W5100_CR_OFFSET))  ;			// loop until device reports socket is open (blocks!!)
}

unsigned char  OpenSocket(unsigned char  sock, unsigned char  eth_protocol, unsigned int  tcp_port)
{
    unsigned char			retval;
	unsigned int			sockaddr;
  unsigned int status;

	retval = W5100_FAIL;							// assume this doesn't work
    if (sock >= W5100_NUM_SOCKETS)  return retval;	// illegal socket value is bad!

	sockaddr =  W5100_SKT_BASE(sock);				// calc base addr for this socket

    if (W51_read(sockaddr+W5100_SR_OFFSET) == W5100_SKT_SR_CLOSED)    // Make sure we close the socket first
	{
		CloseSocket(sock);
    }

    W51_write(sockaddr+W5100_MR_OFFSET ,eth_protocol);		// set protocol for this socket
    W51_write(sockaddr+W5100_PORT_OFFSET, ((tcp_port & 0xFF00) >> 8 ));		// set port for this socket (MSB)
    W51_write(sockaddr+W5100_PORT_OFFSET + 1, (tcp_port & 0x00FF));			// set port for this socket (LSB)
    W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_OPEN);	               	// open the socket

    while (W51_read(sockaddr+W5100_CR_OFFSET))  ;			// loop until device reports socket is open (blocks!!)

    status = W51_read(sockaddr+W5100_SR_OFFSET);
    if (status == W5100_SKT_SR_INIT || status == W5100_SKT_SR_UDP)  retval = sock;	// if success, return socket number
    else  CloseSocket(sock);								// if failed, close socket immediately

    return  retval;
}

void  CloseSocket(unsigned char  sock)
{
	unsigned int			sockaddr;

	if (sock > W5100_NUM_SOCKETS)  return;			// if illegal socket number, ignore request
	sockaddr = W5100_SKT_BASE(sock);				// calc base addr for this socket

	W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_CLOSE);	// tell chip to close the socket
	while (W51_read(sockaddr+W5100_CR_OFFSET))  ;	// loop until socket is closed (blocks!!)
}

void  DisconnectSocket(unsigned char  sock)
{
	unsigned int			sockaddr;

	if (sock > W5100_NUM_SOCKETS)  return;			// if illegal socket number, ignore request
	sockaddr = W5100_SKT_BASE(sock);				// calc base addr for this socket

	W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_DISCON);		// disconnect the socket
	while (W51_read(sockaddr+W5100_CR_OFFSET))  ;	// loop until socket is closed (blocks!!)
}




unsigned char  Listen(unsigned char  sock)
{
	unsigned char			retval;
	unsigned int			sockaddr;

	retval = W5100_FAIL;							// assume this fails	
	if (sock > W5100_NUM_SOCKETS)  return  retval;	// if illegal socket number, ignore request

	sockaddr = W5100_SKT_BASE(sock);				// calc base addr for this socket
	if (W51_read(sockaddr+W5100_SR_OFFSET) == W5100_SKT_SR_INIT)	// if socket is in initialized state...
	{
		W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_LISTEN);		// put socket in listen state
		while (W51_read(sockaddr+W5100_CR_OFFSET))  ;		// block until command is accepted

		if (W51_read(sockaddr+W5100_SR_OFFSET) == W5100_SKT_SR_LISTEN)  retval = W5100_OK;	// if socket state changed, show success
		else  CloseSocket(sock);					// not in listen mode, close and show an error occurred
    }
    return  retval;
}

unsigned char  Send(unsigned char  sock, const unsigned char  *buf, unsigned int  buflen)
{
	unsigned int					ptr;
	unsigned int					offaddr;
	unsigned int					realaddr;
	unsigned int					txsize;
	unsigned int					timeout;   
	unsigned int					sockaddr;

    if (buflen == 0 || sock >= W5100_NUM_SOCKETS)  return  W5100_FAIL;		// ignore illegal requests
	sockaddr = W5100_SKT_BASE(sock);			// calc base addr for this socket
    // Make sure the TX Free Size Register is available
	txsize = W51_read(sockaddr+W5100_TX_FSR_OFFSET);		// make sure the TX free-size reg is available
    txsize = (((txsize & 0x00FF) << 8 ) + W51_read(sockaddr+W5100_TX_FSR_OFFSET + 1));

    timeout = 0;
    while (txsize < buflen)
	{
		_delay_ms(1);

		txsize = W51_read(sockaddr+W5100_TX_FSR_OFFSET);		// make sure the TX free-size reg is available
    	txsize = (((txsize & 0x00FF) << 8 ) + W51_read(sockaddr+W5100_TX_FSR_OFFSET + 1));

		if (timeout++ > 1000) 						// if max delay has passed...
		{
			DisconnectSocket(sock);					// can't connect, close it down
			return  W5100_FAIL;						// show failure
		}
	}	

   // Read the Tx Write Pointer
	ptr = W51_read(sockaddr+W5100_TX_WR_OFFSET);
	offaddr = (((ptr & 0x00FF) << 8 ) + W51_read(sockaddr+W5100_TX_WR_OFFSET + 1));

    while (buflen)
	{
		buflen--;
		realaddr = W5100_TXBUFADDR + (offaddr & W5100_TX_BUF_MASK);		// calc W5100 physical buffer addr for this socket

		W51_write(realaddr, *buf);					// send a byte of application data to TX buffer
		offaddr++;									// next TX buffer addr
		buf++;										// next input buffer addr
    }

    W51_write(sockaddr+W5100_TX_WR_OFFSET, (offaddr & 0xFF00) >> 8);	// send MSB of new write-pointer addr
    W51_write(sockaddr+W5100_TX_WR_OFFSET + 1, (offaddr & 0x00FF));		// send LSB	

    W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_SEND);	// start the send on its way
	while (W51_read(sockaddr+W5100_CR_OFFSET))  ;	// loop until socket starts the send (blocks!!)

    return  W5100_OK;
}


void UDPOpen(unsigned char  sock, unsigned char ip_1, unsigned char ip_2, unsigned char ip_3, unsigned char ip_4, unsigned int udp_port) {
    unsigned int sockaddr = W5100_SKT_BASE(sock);			// calc base addr for this socket
    W51_write(sockaddr+W5100_DIPR_OFFSET, ip_1);		// set destination ip for this socket
    W51_write(sockaddr+W5100_DIPR_OFFSET + 1, ip_2);
    W51_write(sockaddr+W5100_DIPR_OFFSET + 2, ip_3);
    W51_write(sockaddr+W5100_DIPR_OFFSET + 3, ip_4);
    W51_write(sockaddr+W5100_DPORT_OFFSET, ((udp_port & 0xFF00) >> 8 ));		// set destination port for this socket (MSB)
    W51_write(sockaddr+W5100_DPORT_OFFSET + 1, (udp_port & 0x00FF));			// set destination port for this socket (LSB)
}

unsigned int  Receive(unsigned char  sock, unsigned char  *buf, unsigned int  buflen)
{
    unsigned int					ptr;
	unsigned int					offaddr;
	unsigned int					realaddr;   	
	unsigned int					sockaddr;

    if (buflen == 0 || sock >= W5100_NUM_SOCKETS)  return  W5100_FAIL;		// ignore illegal conditions

	if (buflen > (MAX_BUF-2))  buflen = MAX_BUF - 2;		// requests that exceed the max are truncated

	sockaddr = W5100_SKT_BASE(sock);						// calc base addr for this socket
	ptr = W51_read(sockaddr+W5100_RX_RD_OFFSET);			// get the RX read pointer (MSB)
    offaddr = (((ptr & 0x00FF) << 8 ) + W51_read(sockaddr+W5100_RX_RD_OFFSET + 1));		// get LSB and calc offset addr

	while (buflen)
	{
		buflen--;
		realaddr = W5100_RXBUFADDR + (offaddr & W5100_RX_BUF_MASK);
		*buf = W51_read(realaddr);
		offaddr++;
		buf++;
	}
	*buf='\0'; 												// buffer read is complete, terminate the string

    // Increase the S0_RX_RD value, so it point to the next receive
	W51_write(sockaddr+W5100_RX_RD_OFFSET, (offaddr & 0xFF00) >> 8);	// update RX read offset (MSB)
    W51_write(sockaddr+W5100_RX_RD_OFFSET + 1,(offaddr & 0x00FF));		// update LSB

    // Now Send the RECV command
    W51_write(sockaddr+W5100_CR_OFFSET, W5100_SKT_CR_RECV);			// issue the receive command
    _delay_us(5);											// wait for receive to start

    return  W5100_OK;
}




unsigned int  ReceivedSize(unsigned char  sock)
{
	unsigned int					val;
	unsigned int					sockaddr;

	if (sock >= W5100_NUM_SOCKETS)  return  0;
	sockaddr = W5100_SKT_BASE(sock);						// calc base addr for this socket
	val = W51_read(sockaddr+W5100_RX_RSR_OFFSET) & 0xff;
	val = (val << 8) + W51_read(sockaddr+W5100_RX_RSR_OFFSET + 1);
	return  val;
}

// magic number! declare the socket number we will use (0-3)
#define MYSOCKET 0
// calc address of W5100 register set for this socket
#define SOCKADDR W5100_SKT_BASE(MYSOCKET)

void ethernet_setup(void)
{

	W51_init();												// now initialize the W5100

/*
 *  Configure the W5100 device to handle PING requests.
 *  This requires configuring the chip, not a specific socket.
 */
 	W51_config();									// config the W5100 (MAC, TCP address, subnet, etc
}

/*
 *  The main loop.  Control stays in this loop forever, processing any received packets
 *  and sending any requested data.
 */
void ethernet_loop(void) {
	unsigned int					rsize;
		switch  (W51_read(SOCKADDR+W5100_SR_OFFSET))		// based on current status of socket...
		{
			case  W5100_SKT_SR_CLOSED:						// if socket is closed...
			if (OpenSocket(MYSOCKET, W5100_SKT_MR_TCP, 555) == MYSOCKET)		// if successful opening a socket...
			{
				Listen(MYSOCKET);
				_delay_ms(1);
			}
			break;

			case  W5100_SKT_SR_ESTABLISHED:					// if socket connection is established...
			rsize = ReceivedSize(MYSOCKET);					// find out how many bytes
			if (rsize > 0)
			{
				if (Receive(MYSOCKET, buf, rsize) != W5100_OK)  break;	// if we had problems, all done
/*
 *  Add code here to process the payload from the packet.
 */
				strcpy_P((char *)buf, PSTR("Hello World\r\n"));
				strcpy_P((char *)buf, PSTR("This is LED-Matrix!\r\n"));
				if (Send(MYSOCKET, buf, strlen((char *)buf)) == W5100_FAIL)  break;		// just throw out the packet for now

				DisconnectSocket(MYSOCKET);
			}
			else											// no data yet...
			{
				_delay_us(10);
			}
			break;

			case  W5100_SKT_SR_FIN_WAIT:
			case  W5100_SKT_SR_CLOSING:
			case  W5100_SKT_SR_TIME_WAIT:
			case  W5100_SKT_SR_CLOSE_WAIT:
			case  W5100_SKT_SR_LAST_ACK:
			CloseSocket(MYSOCKET);
			break;
		}
}
