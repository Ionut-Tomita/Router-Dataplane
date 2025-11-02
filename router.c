#include "queue.h"
#include <arpa/inet.h> /* ntoh, hton and inet_ functions */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include "lib.h"
#include "protocols.h"
#include <string.h>

#define ICMP_LEN 98

// Routing table
struct route_table_entry *rtable;
int rtable_len;

// ARP table
struct arp_table_entry *arp_table;
int arp_table_len;

// send back an ICMP packet
void icmp(int interface, char *buffer, uint8_t type) {

	char packet[MAX_PACKET_LEN];

	struct ether_header *eth_hdr = (struct ether_header *)packet;
	struct iphdr *ip_hdr = (struct iphdr *)(packet + sizeof(struct ether_header));
	struct icmphdr *icmp_hdr = (struct icmphdr *)(packet + sizeof(struct ether_header) + sizeof(struct iphdr));

	memcpy(packet + sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr), buffer + sizeof(struct ether_header), ICMP_LEN);

	// Ethernet header

	// swap source and destination eth_hdr
	uint8_t aux[6];
	memcpy(aux, eth_hdr->ether_dhost, 6);
	memcpy(eth_hdr->ether_dhost, eth_hdr->ether_shost, 6);
	memcpy(eth_hdr->ether_shost, aux, 6);
	
	eth_hdr->ether_type = htons(ETHERTYPE_IP);

	// IP header
	memcpy(ip_hdr, (char *)(buffer + sizeof(struct ether_header)), sizeof(struct iphdr));
	
	ip_hdr->ttl = 64;
	ip_hdr->protocol = IPPROTO_ICMP;
	ip_hdr->daddr = ip_hdr->saddr;
	ip_hdr->saddr = inet_addr(get_interface_ip(interface));
	ip_hdr->tot_len = htons(sizeof(struct ether_header) + sizeof(struct iphdr) + sizeof(struct icmphdr) + sizeof(packet));
	ip_hdr->frag_off = 0;
	ip_hdr->tos = 0;
	ip_hdr->id = htons(1);
	ip_hdr->ihl = 5;
	ip_hdr->check = 0;
	ip_hdr->check = htons(checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)));

	// icmp header
	icmp_hdr->code = 0;
	icmp_hdr->type = type;
	icmp_hdr->checksum = 0;
	icmp_hdr->checksum = htons(checksum((uint16_t*)icmp_hdr, ntohs(ip_hdr->tot_len) - sizeof(struct iphdr)));

	send_to_link(interface, packet, ICMP_LEN + 64);
}

struct route_table_entry *binary_search(uint32_t ip_dest, int left, int right) {
	if (left > right) {
		return NULL;
	}

	int mid = left + (right - left) / 2;

	if ((ip_dest & rtable[mid].mask) == rtable[mid].prefix) {

		struct route_table_entry *best_route = &rtable[mid];
		// search for a more specific route
		for (int i = mid + 1; i < rtable_len ; i++) {
			
			if ((ip_dest & rtable[i].mask) == rtable[i].prefix) {
				best_route = &rtable[i];
			}
		}
		return best_route;

	} else if ((ip_dest & rtable[mid].mask) > rtable[mid].prefix) {

		// search in the right side
		return binary_search(ip_dest, mid + 1, right);

	} else {
		// search in the left side
		return binary_search(ip_dest, left, mid - 1);
	}
}

struct route_table_entry *get_best_route(uint32_t ip_dest) {

	return binary_search(ip_dest, 0, rtable_len - 1);
}


struct arp_table_entry *get_arp_entry(uint32_t given_ip) {
	
	// Search for an entry that matches given_ip
	for (int i = 0; i < arp_table_len; i++) {
		if (arp_table[i].ip == given_ip) {
			return &arp_table[i];
		}
	}
	return NULL;
}


int cmpfunc(const void *a, const void *b) {
    struct route_table_entry *route_a = (struct route_table_entry *)a;
    struct route_table_entry *route_b = (struct route_table_entry *)b;

    if (route_a->mask < route_b->mask) {
        return -1;
    } else if (route_a->mask > route_b->mask) {
        return 1;
    } else {
        return 0;
    }
}


int main(int argc, char *argv[])
{
	int interface;
	char buffer[MAX_PACKET_LEN];
	size_t len;

	init(argc - 2, argv + 2);

	// Code to allocate the ARP and route tables
	rtable = malloc(sizeof(struct route_table_entry) * 100000);
	DIE(rtable == NULL, "memory");

	arp_table = malloc(sizeof(struct  arp_table_entry) * 100000);
	DIE(arp_table == NULL, "memory");
	
	// Read the static routing table and the ARP table
	rtable_len = read_rtable(argv[1], rtable);
	arp_table_len = parse_arp_table("arp_table.txt", arp_table);

	// sort the routing table
	qsort(rtable, rtable_len, sizeof(struct route_table_entry), cmpfunc);


	while (1) {

		interface = recv_from_any_link(buffer, &len);
		DIE(interface < 0, "recv_from_any_links");

		struct ether_header *eth_hdr = (struct ether_header *) buffer;
		struct iphdr *ip_hdr = (struct iphdr *)(buffer + sizeof(struct ether_header));

		// Check if we got an IPv4 packet
		if (eth_hdr->ether_type != ntohs(ETHERTYPE_IP)) {
			continue;
		} else {
			
			// router is the destination	
			if (ip_hdr->protocol == IPPROTO_ICMP && inet_addr(get_interface_ip(interface)) == ip_hdr->daddr) {
				icmp(interface, buffer, 0);
				continue;
			}

		}

		// Check the ip_hdr integrity using checksum
		if (checksum((uint16_t *)ip_hdr, sizeof(struct iphdr)) != 0) {
			continue;
		}

		// Check TTL
		if (ip_hdr->ttl <= 1) {

			// time exceeded
			icmp(interface, buffer, 11);
			continue;
		}

		// Find the most specific route
		struct route_table_entry *best_router = get_best_route(ip_hdr->daddr);

		if (best_router == NULL) {

			// host unreachable
			icmp(interface, buffer, 3);
			continue;
		}


		struct arp_table_entry *arp_entry = get_arp_entry(best_router->next_hop);

		// no arp entry found
		if (arp_entry == NULL) {
			continue;
		}

		// Update the ethernet addresses
		for (int i = 0; i < 6; i++) {
			eth_hdr->ether_dhost[i] = arp_entry->mac[i];
		}

		// save the old ttl and checksum
		uint16_t old_ttl = ip_hdr->ttl;
		uint16_t old_checksum = ip_hdr->check;

		// update the ttl and checksum
		ip_hdr->ttl--;
		ip_hdr->check = ~(~old_checksum + ~((u_int16_t)old_ttl) + (uint16_t)ip_hdr->ttl) - 1;
		  
		// get the mac address of the interface
		get_interface_mac(best_router->interface, eth_hdr->ether_shost);

		// Send the packet
		send_to_link(best_router->interface, buffer, len);

	}
}

