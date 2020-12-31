/*
+----------------------------------------+
|           sniffer module               |
|                                        |
|     packet sniffer written using       |
|            raw sockets                 |
|                                        |
|  Author: f0lg0                         |
|  Date: 30-12-2020 (dd-mm-yyyy)         |
+----------------------------------------+
*/

#include "includes.h"
#define BUFF_SIZE 65536

// global pointer defined in 'netmon.c'
// I was lazy so I used a global variable instead of passing the pointer to every function
extern FILE* log_f;

/**
 * open_sock: open a raw socket
 * ! NEED ROOT PERMISSIONS
 * @param void
 * @return raw socket if success, -1 if failure
*/
int open_rsock() {
    printf("[  \033[1;33mLOG\033[0m  ] Opening raw socket...\n");

    int rsock = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL));
    setsockopt(rsock , SOL_SOCKET , SO_BINDTODEVICE , "wlp5s0" , strlen("wlp5s0") + 1);

    if (rsock < 0) return -1;

    printf("[  \033[1;33mLOG\033[0m  ] Raw socket opened.\n");
    return rsock;
}

/**
 * alloc_pckts_buffer: allocate huge buffer for packets
 * @param void
 * @return pointer to the newly allocated buffer
*/
unsigned char* alloc_pckts_buffer() {
    unsigned char* buffer = malloc(BUFF_SIZE);
    bzero(buffer, BUFF_SIZE);

    return buffer;
}

/**
 * print_pckt_payload: prints the data field of a packet to the screen
 * @param buffer memory containing the packets
 * @param brecv amount of data received
 * @param iphdrlen the length (or size) of the IP header
 * @return void
*/
void print_pckt_payload(unsigned char* buffer, ssize_t brecv, unsigned short iphdrlen) {
    // getting packet payload
    unsigned char* data = (buffer + sizeof(struct ethhdr) + iphdrlen + sizeof(struct udphdr));
    int remaining_data = brecv - (sizeof(struct ethhdr) + iphdrlen + sizeof(struct udphdr));
    
    printf("\n\tData\n\t");
    for(int i = 0; i < remaining_data; i++) {
        if(i != 0 && i % 16 == 0) {
            printf("\n\t");
        }
        printf(" %.2X ", data[i]);
    }
    printf("\n");
}

/**
 * print_ethhdr: extracts the ethernet header and prints it
 * @param buffer memory containing the packets
 * @return void
*/
void print_ethhdr(unsigned char* buffer) {
    /*
        Ethernet Packet
        struct ethhdr {
            unsigned char h_dest[ETH_ALEN];
            unsigned char h_source[ETH_ALEN];
            __be16 h_proto; --> packet type ID field
        } __attribute__((packed));
    */
    struct ethhdr* eth = (struct ethhdr *)(buffer);
    printf("\n\t┌─────────────────┐");
    printf("\n\t│ \033[1;31mEthernet Header\033[0m │");
    printf("\n\t└─────────────────┘\n");

    // %.2X --> at least 2 hex digits, if less itis prefixed with 0s 
    printf("\t\t├─ Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
    printf("\t\t├─ Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
    printf("\t\t├─ Protocol : 0x%.2x\n",eth->h_proto);
}

/**
 * print_iphdr: extracts and prints to the screen the IP header (it comes after the Ethernet header)
 * @param buffer memory containing the packets
 * @return void
*/
void print_iphdr(unsigned char* buffer) {
    struct iphdr* iphdr = (struct iphdr *)(buffer + sizeof(struct ethhdr));

    struct sockaddr_in src, dst;
    bzero(&src, sizeof(src));
    bzero(&dst, sizeof(dst));

    src.sin_addr.s_addr = iphdr->saddr;
    dst.sin_addr.s_addr = iphdr->daddr;

    printf("\n\t┌────────────────┐");
    printf("\n\t│   \033[1;36mIP Header\033[0m    │");
    printf("\n\t└────────────────┘\n");
    printf("\t\t├─ Version : %d\n",(unsigned int)iphdr->version);
    printf("\t\t├─ Internet Header Length : %d DWORDS or %d Bytes\n",(unsigned int)iphdr->ihl,((unsigned int)(iphdr->ihl))*4);
    printf("\t\t├─ Type Of Service : %d\n",(unsigned int)iphdr->tos);
    printf("\t\t├─ Total Length : %d Bytes\n",ntohs(iphdr->tot_len));
    printf("\t\t├─ Identification : %d\n",ntohs(iphdr->id));
    printf("\t\t├─ Time To Live : %d\n",(unsigned int)iphdr->ttl);
    printf("\t\t├─ Protocol : %d\n",(unsigned int)iphdr->protocol);
    printf("\t\t├─ Header Checksum : %d\n",ntohs(iphdr->check));
    printf("\t\t├─ Source IP : %s\n", inet_ntoa(src.sin_addr));
    printf("\t\t├─ Destination IP : %s\n",inet_ntoa(dst.sin_addr));
}

/**
 * print_udppckt: prints to the screen a UPD packet
 * @param buffer memory containing the packets
 * @param brecv the amount of data received
 * @return void
*/
void print_udppckt(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    // getting UDP header
    struct udphdr* udph =(struct udphdr *)(buffer + sizeof(struct ethhdr) + iphdrlen); 

    printf("\n\t┌────────────────┐");
    printf("\n\t│   \033[1;35mUDP Header\033[0m   │");
    printf("\n\t└────────────────┘\n");
    printf("\t\t├─ Source Port : %d\n" , ntohs(udph->source));
    printf("\t\t├─ Destination Port : %d\n" , ntohs(udph->dest));
    printf("\t\t├─ UDP Length : %d\n" , ntohs(udph->len));
    printf("\t\t├─ UDP Checksum : %d\n" , ntohs(udph->check));

    print_pckt_payload(buffer, brecv, iphdrlen);
}

/**
 * print_tcppckt: prints to the screen a TCP packet
 * @param buffer memory containing the packets
 * @param brecv the amount of data received
*/
void print_tcppckt(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    struct tcphdr* tcph = (struct tcphdr *)(buffer + sizeof(struct ethhdr) + iphdrlen);
    
    printf("\n\t┌────────────────┐");
    printf("\n\t│   \033[1;33mTCP Header\033[0m   │");
    printf("\n\t└────────────────┘\n");
	printf("\t\t├─ Source Port      : %u\n", ntohs(tcph->source));
	printf("\t\t├─ Destination Port : %u\n", ntohs(tcph->dest));
	printf("\t\t├─ Sequence Number    : %u\n", ntohl(tcph->seq));
	printf("\t\t├─ Acknowledge Number : %u\n", ntohl(tcph->ack_seq));
	printf("\t\t├─ Header Length      : %d DWORDS or %d BYTES\n" , (unsigned int)tcph->doff,(unsigned int)tcph->doff*4);
	printf("\t\t├─ Urgent Flag          : %d\n", (unsigned int)tcph->urg);
	printf("\t\t├─ Acknowledgement Flag : %d\n", (unsigned int)tcph->ack);
	printf("\t\t├─ Push Flag            : %d\n", (unsigned int)tcph->psh);
	printf("\t\t├─ Reset Flag           : %d\n", (unsigned int)tcph->rst);
	printf("\t\t├─ Synchronise Flag     : %d\n", (unsigned int)tcph->syn);
	printf("\t\t├─ Finish Flag          : %d\n", (unsigned int)tcph->fin);
	printf("\t\t├─ Window         : %d\n",ntohs(tcph->window));
	printf("\t\t├─ Checksum       : %d\n",ntohs(tcph->check));
	printf("\t\t├─ Urgent Pointer : %d\n",tcph->urg_ptr);

    print_pckt_payload(buffer, brecv, iphdrlen);
}

/**
 * print_icmppckt: prints to the screen a ICMP packet
 * @param buffer memory containing the packets
 * @param brecv the amount of data received
 * @return void
*/
void print_icmppckt(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    struct icmphdr* icmph = (struct icmphdr *)(buffer + sizeof(struct ethhdr) + iphdrlen); 

    printf("\n\t┌────────────────┐");
    printf("\n\t│   \033[1;34mICMP Header\033[0m  │");
    printf("\n\t└────────────────┘\n");
	printf("\t\t├─ Type : %d", (unsigned int)(icmph->type));
			
	if ((unsigned int)(icmph->type) == 11) {
		printf("\t(TTL Expired)\n");
	} else if ((unsigned int)(icmph->type) == ICMP_ECHOREPLY) {
		printf("\t(ICMP Echo Reply)\n");
	}

    printf("\t\t├─ Code : %d\n", (unsigned int)(icmph->code));
	printf("\t\t├─ Checksum : %d\n", ntohs(icmph->checksum));

    // Printing Data
    unsigned char* data = (buffer + sizeof(struct ethhdr) + iphdrlen + sizeof(struct icmphdr));
    int remaining_data = brecv - (sizeof(struct ethhdr) + iphdrlen + sizeof(struct icmphdr));

    print_pckt_payload(buffer, brecv, iphdrlen);
}

/**
 * recv_net_pckts: stream net packets
 * @param rsock pointer to a raw socket
 * @param buffer memory to store packets
 * @param saddr pointer to an instance of the sockaddr structure
 * @param saddrlen size of the saddr instance
 * @return received bytes if success, -1 if failure
*/
ssize_t recv_net_pckts(int* rsock, unsigned char* buffer, struct sockaddr* saddr, size_t saddrlen) {
    // using recvfrom because "it permits the application to retrieve the source address of received data" (man)
    ssize_t brecv = recvfrom(*rsock, buffer, BUFF_SIZE, 0, saddr, (socklen_t *)&saddrlen);

    if (brecv < 0) {
        printf("[ERROR] (recv_net_pckts: %d) Failed to receive data.\n", brecv);
        return -1;
    }
    return brecv;
}

/**
 * openlog: open log file to dump packets
 * @return 0 if success, 1 if failure
*/
int openlog() {
    log_f = fopen("log.txt", "a");
    if (!log_f) return 1;

    return 0;
}


/**
 * dump_ethhdr_to_log: dump sniffed eth header to log file
 * @param buffer memory containing the packets
 * @return void
*/
void dump_ethhdr_to_log(unsigned char* buffer) {
    struct ethhdr* eth = (struct ethhdr *)(buffer);

    fprintf(log_f, "\n\tEthernet Header\n");
    fprintf(log_f, "\t\t├─ Source Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_source[0],eth->h_source[1],eth->h_source[2],eth->h_source[3],eth->h_source[4],eth->h_source[5]);
    fprintf(log_f, "\t\t├─ Destination Address : %.2X-%.2X-%.2X-%.2X-%.2X-%.2X\n",eth->h_dest[0],eth->h_dest[1],eth->h_dest[2],eth->h_dest[3],eth->h_dest[4],eth->h_dest[5]);
    fprintf(log_f, "\t\t├─ Protocol : 0x%.2x\n",eth->h_proto);
    
}

/**
 * dump_iphdr_to_log: dump sniffed IP header to log file
 * @param buffer memory containing the packets
 * @return void
*/
void dump_iphdr_to_log(unsigned char* buffer) {
    struct iphdr* iphdr = (struct iphdr *)(buffer + sizeof(struct ethhdr));

    struct sockaddr_in src, dst;
    bzero(&src, sizeof(src));
    bzero(&dst, sizeof(dst));

    src.sin_addr.s_addr = iphdr->saddr;
    dst.sin_addr.s_addr = iphdr->daddr;

    fprintf(log_f, "\n\tIP Header\n");
    fprintf(log_f, "\t\t├─ Version : %d\n",(unsigned int)iphdr->version);
    fprintf(log_f, "\t\t├─ Internet Header Length : %d DWORDS or %d Bytes\n",(unsigned int)iphdr->ihl,((unsigned int)(iphdr->ihl))*4);
    fprintf(log_f, "\t\t├─ Type Of Service : %d\n",(unsigned int)iphdr->tos);
    fprintf(log_f, "\t\t├─ Total Length : %d Bytes\n",ntohs(iphdr->tot_len));
    fprintf(log_f, "\t\t├─ Identification : %d\n",ntohs(iphdr->id));
    fprintf(log_f, "\t\t├─ Time To Live : %d\n",(unsigned int)iphdr->ttl);
    fprintf(log_f, "\t\t├─ Protocol : %d\n",(unsigned int)iphdr->protocol);
    fprintf(log_f, "\t\t├─ Header Checksum : %d\n",ntohs(iphdr->check));
    fprintf(log_f, "\t\t├─ Source IP : %s\n", inet_ntoa(src.sin_addr));
    fprintf(log_f, "\t\t├─ Destination IP : %s\n",inet_ntoa(dst.sin_addr));
}

/**
 * dump_pckt_payload_to_log: dump sniffed packet payload to log file
 * @param buffer memoery containing the packets
 * @param brecv amount of data received
 * @param iphdrlen length (or size) of the IP header
 * @return void
*/
void dump_pckt_payload_to_log(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    // getting packet payload
    unsigned char* data = (buffer + sizeof(struct ethhdr) + iphdrlen + sizeof(struct udphdr));
    int remaining_data = brecv - (sizeof(struct ethhdr) + iphdrlen + sizeof(struct udphdr));
    
    fprintf(log_f, "\n\tData\n\t");
    for(int i = 0; i < remaining_data; i++) {
        if(i != 0 && i % 16 == 0) {
            fprintf(log_f, "\n\t");
        }
        fprintf(log_f, " %.2X ", data[i]);
    }
    fprintf(log_f, "\n");
}

/**
 * dump_icmppckt_to_log: dump sniffed ICMP packet to log file
 * @param buffer memory containing the packets
 * @param brecv amount of data received
 * @param iphdrlen the length (or size) of the IP header
 * @return void
*/
void dump_icmppckt_to_log(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    struct icmphdr* icmph = (struct icmphdr *)(buffer + sizeof(struct ethhdr) + iphdrlen); 

    fprintf(log_f, "\n\tICMP Header\n");
	fprintf(log_f, "\t\t├─ Type : %d", (unsigned int)(icmph->type));
			
	if ((unsigned int)(icmph->type) == 11) {
		fprintf(log_f, "\t(TTL Expired)\n");
	} else if ((unsigned int)(icmph->type) == ICMP_ECHOREPLY) {
		fprintf(log_f, "\t(ICMP Echo Reply)\n");
	}

    fprintf(log_f, "\t\t├─ Code : %d\n", (unsigned int)(icmph->code));
	fprintf(log_f, "\t\t├─ Checksum : %d\n", ntohs(icmph->checksum));

    unsigned char* data = (buffer + sizeof(struct ethhdr) + iphdrlen + sizeof(struct icmphdr));
    int remaining_data = brecv - (sizeof(struct ethhdr) + iphdrlen + sizeof(struct icmphdr));

    dump_pckt_payload_to_log(buffer, brecv, iphdrlen);
}

/**
 * dump_tcppckt_to_log: dump sniffed TCP packet to log file
 * @param buffer memory containing the packets
 * @param brecv amount of data received
 * @param iphdrlen the length (or size) of the IP header
 * @return void
*/
void dump_tcppckt_to_log(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    struct tcphdr* tcph = (struct tcphdr *)(buffer + sizeof(struct ethhdr) + iphdrlen);
    
    fprintf(log_f, "\n\tTCP Header\n");
	fprintf(log_f, "\t\t├─ Source Port      : %u\n", ntohs(tcph->source));
	fprintf(log_f, "\t\t├─ Destination Port : %u\n", ntohs(tcph->dest));
	fprintf(log_f, "\t\t├─ Sequence Number    : %u\n", ntohl(tcph->seq));
	fprintf(log_f, "\t\t├─ Acknowledge Number : %u\n", ntohl(tcph->ack_seq));
	fprintf(log_f, "\t\t├─ Header Length      : %d DWORDS or %d BYTES\n" , (unsigned int)tcph->doff,(unsigned int)tcph->doff*4);
	fprintf(log_f, "\t\t├─ Urgent Flag          : %d\n", (unsigned int)tcph->urg);
	fprintf(log_f, "\t\t├─ Acknowledgement Flag : %d\n", (unsigned int)tcph->ack);
	fprintf(log_f, "\t\t├─ Push Flag            : %d\n", (unsigned int)tcph->psh);
	fprintf(log_f, "\t\t├─ Reset Flag           : %d\n", (unsigned int)tcph->rst);
	fprintf(log_f, "\t\t├─ Synchronise Flag     : %d\n", (unsigned int)tcph->syn);
	fprintf(log_f, "\t\t├─ Finish Flag          : %d\n", (unsigned int)tcph->fin);
	fprintf(log_f, "\t\t├─ Window         : %d\n",ntohs(tcph->window));
	fprintf(log_f, "\t\t├─ Checksum       : %d\n",ntohs(tcph->check));
	fprintf(log_f, "\t\t├─ Urgent Pointer : %d\n",tcph->urg_ptr);

    dump_pckt_payload_to_log(buffer, brecv, iphdrlen);
}
/**
 * dump_udppckt_to_log: dump sniffed UDP packet to log file
 * @param buffer memory containing the packets
 * @param brecv amount of data received
 * @param iphdrlen the length (or size) of the IP header
 * @return void
*/
void dump_udppckt_to_log(unsigned char* buffer, ssize_t brecv, unsigned int iphdrlen) {
    // getting UDP header
    struct udphdr* udph =(struct udphdr *)(buffer + sizeof(struct ethhdr) + iphdrlen); 

    fprintf(log_f, "\n\tUDP Header\n");
    fprintf(log_f, "\t\t├─ Source Port : %d\n" , ntohs(udph->source));
    fprintf(log_f, "\t\t├─ Destination Port : %d\n" , ntohs(udph->dest));
    fprintf(log_f, "\t\t├─ UDP Length : %d\n" , ntohs(udph->len));
    fprintf(log_f, "\t\t├─ UDP Checksum : %d\n" , ntohs(udph->check));

    dump_pckt_payload_to_log(buffer, brecv, iphdrlen);
}

/**
 * process_pcket: sorts an incoming packet
 * @param buffer memory containing packets
 * @param brecv amount of data received
 * @param totpckts the number of packets received (increases)
*/
void process_pcket(unsigned char* buffer, ssize_t brecv, int totpckts, int logfile) {
    // getting the IP header
    struct iphdr* iphdr = (struct iphdr *)(buffer + sizeof(struct ethhdr));
    
    // getting size from IHL (Internet Header Length), which is the number of 32-bit words.
    // multiply by 4 to get the size in bytes
    unsigned int iphdrlen = iphdr->ihl * 4;

    if (logfile == 0) {
        printf("\n\033[1;32m[>] Sniffed Packet #%d\033[0m\n", totpckts);

        print_ethhdr(buffer);
        print_iphdr(buffer);

        /* vim /etc/protocols */
        switch (iphdr->protocol) {
            // ICMP
            case 1: 
                print_icmppckt(buffer, brecv, iphdrlen);
                break;

            // TCP
            case 6: 
                print_tcppckt(buffer, brecv, iphdrlen);
                break;

            // UDP
            case 17:
                print_udppckt(buffer, brecv, iphdrlen);
                break;
            
            default:
                break;
        }
    } else {
        fprintf(log_f, "\n[>] Sniffed Packet #%d\n", totpckts);
        dump_ethhdr_to_log(buffer);
        dump_iphdr_to_log(buffer);

        /* vim /etc/protocols */
        switch (iphdr->protocol) {
            // ICMP
            case 1: 
                dump_icmppckt_to_log(buffer, brecv, iphdrlen);
                break;

            // TCP
            case 6: 
                dump_tcppckt_to_log(buffer, brecv, iphdrlen);
                break;

            // UDP
            case 17:
                dump_udppckt_to_log(buffer, brecv, iphdrlen);
                break;
            
            default:
                break;
        }

        fflush(log_f);
    }

}

/**
 * run_sniffer: receive network packets
 * @param rsock pointer to a raw socket file descriptor
 * @return 0 if success, -1 if failure
*/
int run_sniffer(int* rsock, unsigned char* buffer, int pckts_num, int logfile) {
    if (logfile != 0 && logfile != 1) {
        return -1;
    } else if (logfile == 1) {
        openlog();
    };

    struct sockaddr saddr;
    struct sockaddr* p_saddr = &saddr;
    size_t saddrlen = sizeof(saddr);

    ssize_t brecv;
    if (pckts_num > 0) {
        for (int i = 0; i < pckts_num; i++) {
            if ((brecv = recv_net_pckts(rsock, buffer, p_saddr, saddrlen)) == -1) return -1;
            process_pcket(buffer, brecv, i, logfile);
        }
    } else if (pckts_num == 0) {
        int i = 0;
        while (1) {
            if ((brecv = recv_net_pckts(rsock, buffer, p_saddr, saddrlen)) == -1) return -1;
            process_pcket(buffer, brecv, i, logfile);
            i++;
        }
    } else {
        return -1;
    }
    
    free(buffer);
    return 0;
}
