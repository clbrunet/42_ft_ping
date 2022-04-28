#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <errno.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip.h>
#include <signal.h>

#include "ft_ping/main.h"
#include "ft_ping/utils/print.h"
#include "ft_ping/initialize.h"

variables_t g_vars = {0};

void int_handler(int signum)
{
	(void)signum;
	write(STDOUT_FILENO, "\nEND\n", 5);
	free(g_vars.icmp_echo_packet);
	close(g_vars.socket_fd);
	exit(0);
}

int main(int argc, char *argv[])
{
	if (argc == 1) {
		print_error(argv[0], "usage error", "Destination address required");
		return 1;
	}
	if (initialize_socket(&g_vars.socket_fd, argv[0]) == -1) {
		return 2;
	}
	if (initialize_sockaddr_in(&g_vars.destination_sockaddr_in, argv[1], argv[0]) == -1) {
		close(g_vars.socket_fd);
		return 2;
	}
	g_vars.icmp_echo_packet_id = getpid() & UINT16_MAX;
	g_vars.icmp_echo_packet_payload_size = 56;
	g_vars.icmp_echo_packet = malloc(sizeof(struct icmphdr) + g_vars.icmp_echo_packet_payload_size);
	if (g_vars.icmp_echo_packet == NULL) {
		print_error(argv[0], "malloc", strerror(errno));
		close(g_vars.socket_fd);
		return 2;
	}
	if (initialize_icmp_echo_packet(g_vars.icmp_echo_packet,
			g_vars.icmp_echo_packet_payload_size, g_vars.icmp_echo_packet_id, 1, argv[0]) == -1) {
		free(g_vars.icmp_echo_packet);
		close(g_vars.socket_fd);
		return 2;
	}

	if (sendto(g_vars.socket_fd, g_vars.icmp_echo_packet,
			sizeof(struct icmphdr) + g_vars.icmp_echo_packet_payload_size, 0,
			(const struct sockaddr *)&g_vars.destination_sockaddr_in, sizeof(struct sockaddr_in))
		== -1) {
		print_error(argv[0], "sendto", strerror(errno));
		free(g_vars.icmp_echo_packet);
		close(g_vars.socket_fd);
		return 2;
	}
	printf("ICMP ECHO REQUEST sent\n");

	signal(SIGINT, &int_handler);

	uint8_t msg_iov_base_buf[1000] = {0};
	struct iovec msg_iov[1] = {
		[0].iov_base = msg_iov_base_buf,
		[0].iov_len = 1000,
	};
	struct msghdr msg = {
		.msg_iov = msg_iov,
		.msg_iovlen = 1,
	};

	while (true) {
		ssize_t ret = recvmsg(g_vars.socket_fd, &msg, 0);
		if (ret == -1) {
			print_error(argv[0], "recvmsg", strerror(errno));
			free(g_vars.icmp_echo_packet);
			close(g_vars.socket_fd);
			return 2;
		}
		printf("\nMessage received\n");
		print_memory(msg_iov_base_buf, ret);
		struct iphdr *response_iphdr = (void *)msg_iov_base_buf;
		printf("IP header\n");
		printf("saddr %u\n", response_iphdr->saddr);
		printf("daddr %u\n", response_iphdr->daddr);
		struct icmphdr *response_icmphdr = (void *)msg_iov_base_buf + 20;
		printf("ICMP header\n");
		printf("type %hhu\n", response_icmphdr->type);
		printf("code %hhu\n", response_icmphdr->code);
		printf("checksum %hu\n", response_icmphdr->checksum);
		printf("id %hu or %hu\n", response_icmphdr->un.echo.id, ntohs(response_icmphdr->un.echo.id));
		printf("seq %hu or %hu\n", response_icmphdr->un.echo.sequence, ntohs(response_icmphdr->un.echo.sequence));
	}
}