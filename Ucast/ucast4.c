#include <arpa/inet.h>
#include <linux/if_packet.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/ether.h>

// Allocating size to different containers
#define HEADER_SIZE		14


void main( void ) {

    while (1) {

    char message[500];
    printf("\nEnter message for payload:\n"); //Get input message for payload.
    scanf ("%[^\n]%*c", message);
    //gets(message);

    while (1) {
    //char message[] = "Hello World. I am MTP.\n";

	int frame_Size = -1;

	int sockfd;
	struct ifreq if_idx; //Initialize.
	struct ifreq if_mac;

	char ifName[IFNAMSIZ];

	strcpy(ifName, "eth1");
	frame_Size = HEADER_SIZE + 1 + sizeof(message);

    int option;
    char temp_mac[6];

    printf("\nEnter option to select MAC address: 0 - ping, 1 - Host 3, 2 - Host 5, 3 - Break.\n");
    scanf("%d",&option);

    if (option == 0) { //destination address option.
        temp_mac[0]=0x02;
        temp_mac[1]=0x64;
        temp_mac[2]=0xd1;
        temp_mac[3]=0x83;
        temp_mac[4]=0x02;
        temp_mac[5]=0x0f;

    }
    else if (option == 1) {
        temp_mac[0]=0x02;
        temp_mac[1]=0x42;
        temp_mac[2]=0xfa;
        temp_mac[3]=0x4c;
        temp_mac[4]=0xc3;
        temp_mac[5]=0xee;

    }
    else if (option == 2) {
        temp_mac[0]=0x02;
        temp_mac[1]=0xf0;
        temp_mac[2]=0x06;
        temp_mac[3]=0xe0;
        temp_mac[4]=0x81;
        temp_mac[5]=0x2a;

    }
    else
        break;
	// creating frame
	uint8_t frame[frame_Size]; //Frame construction.

	struct ether_header *eh = (struct ether_header*)calloc(1, sizeof(struct ether_header));

	struct sockaddr_ll socket_address;

	// Open RAW socket to send on
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) {
		perror("Socket Error");
	}

	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, ifName, IFNAMSIZ - 1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0)
		perror("SIOCGIFINDEX - Misprint Compatibility");

	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, ifName, IFNAMSIZ - 1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0)
		perror("SIOCGIFHWADDR - Either interface is not correct or disconnected");

	/*
	 *  Ethernet Header - 14 bytes
	 *
	 *  6 bytes - Source MAC Address
	 *  6 bytes - Destination MAC Address
	 *  2 bytes - EtherType
	 *
	 */

	eh->ether_shost[0] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[0]; //Fill source address.
	eh->ether_shost[1] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[5];

	eh->ether_dhost[0] = temp_mac[0]; //Fill destination address.
	eh->ether_dhost[1] = temp_mac[1];
	eh->ether_dhost[2] = temp_mac[2];
	eh->ether_dhost[3] = temp_mac[3];
	eh->ether_dhost[4] = temp_mac[4];
	eh->ether_dhost[5] = temp_mac[5];

	eh->ether_type = htons(0x8850); //Frame type.

	// Copying header to frame
	memcpy(frame, eh, sizeof(struct ether_header));

	// Copying Payload (No. of tier addr + x times (tier addr length + tier addr) )
	// Copying payLoad to frame
	memset(frame + sizeof(struct ether_header), 5, 1);

	strncpy(frame + sizeof(struct ether_header) + 1, message, sizeof(message)); //Add message content to frame.

	// Index of the network device
	socket_address.sll_ifindex = if_idx.ifr_ifindex;

	// Address length - 6 bytes
	socket_address.sll_halen = ETH_ALEN;

	// Destination MAC Address
	socket_address.sll_addr[0] = temp_mac[0];
	socket_address.sll_addr[1] = temp_mac[1];
	socket_address.sll_addr[2] = temp_mac[2];
	socket_address.sll_addr[3] = temp_mac[3];
	socket_address.sll_addr[4] = temp_mac[4];
	socket_address.sll_addr[5] = temp_mac[5];

	// Send packet
	if (sendto(sockfd, frame, frame_Size, 0, (struct sockaddr*) &socket_address, sizeof(struct sockaddr_ll)) < 0) { //send frame.
		printf("ERROR: Send failed\n");
	}

	printf("\nFrame sent.\n");

	free(eh);
	close(sockfd); //Close Socket.
    }
    }
}


