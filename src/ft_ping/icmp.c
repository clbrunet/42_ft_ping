#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <errno.h>
#include <netinet/ip_icmp.h>

#include "ft_ping/icmp.h"
#include "ft_ping/main.h"
#include "ft_ping/utils/print.h"
#include "ft_ping/utils/error.h"
#include "ft_ping/checksum.h"

uint8_t *create_icmp_request(uint16_t id, uint16_t sequence, size_t payload_size)
{
	uint8_t *icmp_request = malloc(ICMP_PACKET_SIZE(payload_size));
	if (icmp_request == NULL) {
		print_error("malloc", ft_strerror(errno));
		return NULL;
	}
	struct icmphdr *icmphdr = (struct icmphdr *)icmp_request;
	icmphdr->type = ICMP_ECHO;
	icmphdr->code = 0;
	icmphdr->checksum = 0;
	icmphdr->un.echo.id = id;
	icmphdr->un.echo.sequence = sequence;

	uint8_t *payload = (uint8_t *)(icmphdr + 1);
	int payload_pad_bytes_begin_index = 0;
	if (payload_size >= sizeof(struct timeval)) {
		if (gettimeofday((struct timeval *)payload, NULL) == -1) {
			print_error("gettimeofday", ft_strerror(errno));
			free(icmp_request);
			return NULL;
		}
		payload_pad_bytes_begin_index += sizeof(struct timeval);
	}
	for (size_t i = payload_pad_bytes_begin_index; i < payload_size; i++) {
		payload[i] = (uint8_t)i;
	}

	icmphdr->checksum = get_checksum(icmphdr, ICMP_PACKET_SIZE(payload_size));
	return icmp_request;
}

int update_icmp_request(void)
{
	struct icmphdr *icmphdr = (struct icmphdr *)g_vars.icmp_request;
	assert(icmphdr->type == ICMP_ECHO);
	assert(icmphdr->code == 0);
	icmphdr->un.echo.sequence++;

	uint8_t *payload = (uint8_t *)(icmphdr + 1);
	if (g_vars.icmp_request_payload_size >= sizeof(struct timeval)) {
		if (gettimeofday((struct timeval *)payload, NULL) == -1) {
			print_error("gettimeofday", ft_strerror(errno));
			return -1;
		}
	}

	icmphdr->checksum = 0;
	icmphdr->checksum = get_checksum(icmphdr, ICMP_PACKET_SIZE(g_vars.icmp_request_payload_size));
	return 0;
}

int send_icmp_request(void)
{
	if (sendto(g_vars.socket_fd, g_vars.icmp_request,
			ICMP_PACKET_SIZE(g_vars.icmp_request_payload_size), 0,
			(const struct sockaddr *)&g_vars.destination.sockaddr_in,
			sizeof(struct sockaddr_in)) == -1) {
		print_error("sendto", ft_strerror(errno));
		return -1;
	}
	return 0;
}
