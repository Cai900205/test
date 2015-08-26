/* Copyright (c) 2014 Freescale Semiconductor, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     * Redistributions of source code must retain the above copyright
 *	 notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *	 notice, this list of conditions and the following disclaimer in the
 *	 documentation and/or other materials provided with the distribution.
 *     * Neither the name of Freescale Semiconductor nor the
 *	 names of its contributors may be used to endorse or promote products
 *	 derived from this software without specific prior written permission.
 *
 *
 * ALTERNATIVELY, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") as published by the Free Software
 * Foundation, either version 2 of that License or (at your option) any
 * later version.
 *
 * THIS SOFTWARE IS PROVIDED BY Freescale Semiconductor ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL Freescale Semiconductor BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */



#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <syslog.h>
#include <string.h>

#include <fsl_cpu_hotplug.h>

int main(int argc, char *argv[])
{
	fd_set readfds, master;
	unsigned int recv_app_socket, send_app_socket, recv_cmd_socket;
	struct sockaddr_un recv_app, send_app, recv_cmd;
	char buf[BUF_SIZE_MAX];
	char cmd_buf[CMD_BUF_SIZE_MAX];
	pid_t app_pid[MAX_NUM_APP];
	unsigned char app_pid_mask[MAX_NUM_APP] = {0};
	int index = 0, i;
	pid_t new_pid;
	struct timeval time;
	int fdmax;
	int len, size, ret;
	pid_t pid, sid;

	/* Fork off the parent process */
	pid = fork();
	if (pid < 0)
		exit(EXIT_FAILURE);

	/* If we got a good PID, then
	 * we can exit the parent process. */
	if (pid > 0)
		exit(EXIT_SUCCESS);

	/* Change the file mode mask */
	umask(0);

	/* Create a new SID for the child process */
	sid = setsid();
	if (sid < 0) {
		/* Log the failure */
		exit(EXIT_FAILURE);
	}

	/* Change the current working directory */
	if ((chdir("/")) < 0) {
		/* Log the failure */
		exit(EXIT_FAILURE);
	}

	/* Close out the standard file descriptors */
	close(STDIN_FILENO);
	close(STDOUT_FILENO);
	close(STDERR_FILENO);

	recv_app_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (recv_app_socket < 0) {
		perror("opening datagram socket");
		exit(EXIT_FAILURE);
	}
	send_app_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (send_app_socket < 0) {
		perror("opening datagram socket");
		exit(EXIT_FAILURE);
	}
	recv_cmd_socket = socket(AF_UNIX, SOCK_DGRAM, 0);
	if (recv_cmd_socket < 0) {
		perror("opening datagram socket");
		exit(EXIT_FAILURE);
	}

	recv_app.sun_family = AF_UNIX;
	strcpy(recv_app.sun_path, S_APP_PATH);

	recv_cmd.sun_family = AF_UNIX;
	strcpy(recv_cmd.sun_path, S_CMD_PATH);

	unlink(recv_app.sun_path);
	unlink(recv_cmd.sun_path);

	len = strlen(recv_app.sun_path) + sizeof(recv_app.sun_family) ;
	if (bind(recv_app_socket, (struct sockaddr *)&recv_app, len) == -1) {
		perror("bind failed recv_app");
		exit(EXIT_FAILURE);
	}

	len = strlen(recv_cmd.sun_path) + sizeof(recv_cmd.sun_family) ;
	if (bind(recv_cmd_socket, (struct sockaddr *)&recv_cmd, len) == -1) {
		perror("bind failed recv_cmd");
		exit(EXIT_FAILURE);
	}

	FD_SET(recv_app_socket, &master);
	FD_SET(recv_cmd_socket, &master);

	time.tv_sec = 1;
	time.tv_usec = 500000;

	fdmax = (recv_app_socket > recv_cmd_socket) ? recv_app_socket :
			recv_cmd_socket;

	while (1) {
		readfds = master;
		select(fdmax + 1, &readfds, NULL, NULL, &time);

		/* Read from app_socket to register new apps */
		if (FD_ISSET(recv_app_socket, &readfds)) {

			/* Clear the recv buffer */
			memset(buf, 0, BUF_SIZE_MAX);

			ret = recv(recv_app_socket, buf, BUF_SIZE_MAX - 1, 0);
			if (ret < 0)
				perror("receiving datagram packet");

			/* get pid of app from buf */
			new_pid = atoi(buf);

			index = -1;

			/* Find free index */
			for (i = 0; i < MAX_NUM_APP; i++)
				if (app_pid_mask[i] == 0) {
					index = i;
					break;
				}
			/* If no index free ignore request */
			if (-1 == index)
				continue;
			app_pid[index] = new_pid;
			app_pid_mask[index] = 1;

		} else if (FD_ISSET(recv_cmd_socket, &readfds)) {
			memset(cmd_buf, 0, CMD_BUF_SIZE_MAX);
			memset(buf, 0, BUF_SIZE_MAX);

			/* read from cmd_socket to listen for commands */
			ret = recv(recv_cmd_socket, cmd_buf,
					CMD_BUF_SIZE_MAX - 1, 0);
			if (ret < 0)
				perror("receiving datagram packet");

			/* Broadcast cmd to all the registered apps */
			for (i = 0; i < MAX_NUM_APP; i++) {
				if (app_pid_mask[i] != 1)
					continue;
				sprintf(buf, "/%d", app_pid[i]);
				send_app.sun_family = AF_UNIX;
				strcpy(send_app.sun_path, buf);
				len = strlen(cmd_buf);
				size =  sizeof(send_app);
				ret = sendto(send_app_socket, cmd_buf, len, 0,
					(const struct sockaddr *)&send_app,
					size);
				if (ret == -1) {
					perror("Send failed ");
					/* If we get "connection refused" we
					 * assume thread is dead and remove it
					 * from our list */
					if (errno == 111)
						app_pid_mask[i] = 0;
				}
			}
		}
	}
	close(recv_app_socket);
	exit(EXIT_SUCCESS);
}
