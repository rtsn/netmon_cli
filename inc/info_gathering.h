#include "includes.h"
#include "utils/http_parser.h"

/**
 * hostinfo: information about an host
*/
typedef struct {
    char* hostname;
    char ipstr_v4[INET_ADDRSTRLEN];
    char ipstr_v6[INET6_ADDRSTRLEN];
} hostinfo;

/* [BEGIN] host info gathering */

/**
 * alloc_hinfo: allocate memory for the hinfo struct
 * @param void
 * @return pointer to a new hinfo struct
*/
hostinfo* alloc_hinfo() {
    hostinfo* hinfo = malloc(sizeof(hostinfo));
    hinfo->hostname = NULL;
    hinfo->ipstr_v4[0] = -1;
    hinfo->ipstr_v6[0] = -1;

    return hinfo; 
}

/**
 * showip: get public ip (v4 and v6) from hostname
 * @param host hostname as string
 * @return pointer to hinfo struct about the newly analyzed host
*/
hostinfo* showip(char* host) {
    hostinfo* hinfo = alloc_hinfo();
    hinfo->hostname = malloc(strlen(host));
    strcpy(hinfo->hostname, host);

    struct addrinfo hints, *res, *p;
    int status;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // AF_INET or AF_INET6 to force version
    hints.ai_socktype = SOCK_STREAM;

    if ((status = getaddrinfo(host, NULL, &hints, &res)) != 0) {
	    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
	    return NULL;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        void *addr;

        // get the pointer to the address itself,
        // different fields in IPv4 and IPv6
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *ipv4 = (struct sockaddr_in *)p->ai_addr;
            addr = &(ipv4->sin_addr);
            inet_ntop(p->ai_family, addr, hinfo->ipstr_v4, sizeof(hinfo->ipstr_v4));
        } else {
            struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *)p->ai_addr;
            addr = &(ipv6->sin6_addr);
            inet_ntop(p->ai_family, addr, hinfo->ipstr_v6, sizeof(hinfo->ipstr_v6));
        }
        
    }

    if (hinfo->ipstr_v4[0] == -1) {
        memcpy(hinfo->ipstr_v4, "NULL", sizeof("NULL"));
    } else if (hinfo->ipstr_v6[0] == -1) {
        memcpy(hinfo->ipstr_v6, "NULL", sizeof("NULL"));
    }

    freeaddrinfo(res);

    return hinfo;
};

/* [END] host info gathering */