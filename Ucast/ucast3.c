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
#define ETH_MTP_CTRL    0x8850
#define CTRL_IP		"172"
#define MAX_BUFFER_SIZE        1024

/*
This code for host 3 just receives the frame and prints the message contents. When the node is initiated, it sends a ping message to the
switch for HAT population. Then it goes into receive mode.
*/



void main( void ) {
    int option = 0; //Flag for mode of operation 0 - Send, 1 - Receiving.
    int sockCtrl = 0, recv_len = 0,sockfd = 0; //Initializations.
	uint8_t recvBuffer[MAX_BUFFER_SIZE];
	struct ether_header *eheader = NULL;
	struct sockaddr_ll src_addr;
	socklen_t addr_len = sizeof(src_addr);

    if ((sockCtrl = socket(AF_PACKET, SOCK_RAW, htons (ETH_MTP_CTRL))) < 0) { //Opened socket for receiving.
		perror("Error: MTP socket()");
		exit(1);
	}

	if ((sockfd = socket(AF_PACKET, SOCK_RAW, IPPROTO_RAW)) == -1) { //Opened socket for sending.
		perror("Socket Error");
	}

        while (1) {

    recv_len = recvfrom(sockCtrl, recvBuffer, MAX_BUFFER_SIZE, MSG_DONTWAIT, (struct sockaddr*) &src_addr, &addr_len); //Receive Frame.
	if (recv_len > 0) {
			char recvOnEtherPort[5];
			//printf("\nReceived a packet\n");
			if_indextoname(src_addr.sll_ifindex, recvOnEtherPort);

			// read ethernet header
			eheader = (struct ether_header*)recvBuffer;
			//printf("\nReceived a MTP Unicast packet");



			char ctrlInterface[] = "eth0";

			// ignore frames that are from the control interface.
			if ((strcmp(recvOnEtherPort, ctrlInterface)) == 0) {
				continue;
			} else {
			    if (strncmp(ether_ntoa((struct ether_addr *)&eheader->ether_dhost), "ff:ff:ff:ff:ff:ff", 17) != 0) { //Ignore Broadcast frames.
				printf("\nSource MAC: %s", ether_ntoa((struct ether_addr *) &eheader->ether_shost));
			printf("\nDestination MAC: %s", ether_ntoa((struct ether_addr *)&eheader->ether_dhost));
			printf("\nMessage Type: %x", ntohs(eheader->ether_type));
            printf("\nReceived on port %s", recvOnEtherPort);
            printf("\nReceived Message : "); //Print message.
            int i=15;
            while(recvBuffer[i] != '\0') {
                printf("%c",recvBuffer[i]);
                i++;
            }
            printf("\n");
			    }

		}
	}
	//close(sockCtrl);

	if (option == 0) //Flag for initial sending.
    {
	int frame_Size = -1;

	struct ifreq if_idx;
	struct ifreq if_mac;

	char ifName[IFNAMSIZ];

	strcpy(ifName, "eth1");
	frame_Size = HEADER_SIZE + 1;

    char temp_mac[6];

    //printf("\nEnter option to select MAC address: 0 - ping, 1 - Host 4, 2 - Host 5\n");
    //scanf("%d",&option);


    if (option == 0) { //options to select host, here only ping is used only once.
        temp_mac[0]=0x02;
        temp_mac[1]=0xd0;
        temp_mac[2]=0x0d;
        temp_mac[3]=0xf7;
        temp_mac[4]=0xf9;
        temp_mac[5]=0xa4;

    }
    else if (option == 1) {
        temp_mac[0]=0x02;
        temp_mac[1]=0x0a;
        temp_mac[2]=0x49;
        temp_mac[3]=0x09;
        temp_mac[4]=0x6f;
        temp_mac[5]=0x1a;

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

	eh->ether_shost[0] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[0]; //Fill source address
	eh->ether_shost[1] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[1];
	eh->ether_shost[2] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[2];
	eh->ether_shost[3] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[3];
	eh->ether_shost[4] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[4];
	eh->ether_shost[5] = ((uint8_t *) &if_mac.ifr_hwaddr.sa_data)[5];

	eh->ether_dhost[0] = temp_mac[0]; //Fill selected destination address.
	eh->ether_dhost[1] = temp_mac[1];
	eh->ether_dhost[2] = temp_mac[2];
	eh->ether_dhost[3] = temp_mac[3];
	eh->ether_dhost[4] = temp_mac[4];
	eh->ether_dhost[5] = temp_mac[5];

	eh->ether_type = htons(0x8850); //Frame Type.

	// Copying header to frame
	memcpy(frame, eh, sizeof(struct ether_header));

	// Copying Payload (No. of tier addr + x times (tier addr length + tier addr) )
	// Copying payLoad to frame
	memset(frame + sizeof(struct ether_header), 5, 1); //Payload Type - PING

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
	if (sendto(sockfd, frame, frame_Size, 0, (struct sockaddr*) &socket_address, sizeof(struct sockaddr_ll)) < 0) { //Send Frame.
		printf("ERROR: Send failed\n");
	}

	printf("\nPing Frame sent.\n");

	free(eh);
	close(sockfd); //Close send socket.
	option = 1; //Flag changed for receiving.
	}
    }
    //close(sockCtrl);
}


