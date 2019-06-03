#include <libssh2.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>
#include <fcntl.h>
#include <malloc.h>
#include <algorithm>
#include <3ds.h>
#include "util.hpp"
#include "daisywheel.hpp"
#include "keyboard.hpp"
#include "ui.hpp"

#define EXIT_COMMAND	"exit"

int ssh()
{
    std::string hostname = "";
    std::string username = "";
    std::string password = "";
	std::string port = "";
	int rc;
	int sock;
	int written;
	struct addrinfo hints, *res = nullptr;
	LIBSSH2_SESSION *session;
	LIBSSH2_CHANNEL *channel;
	std::string commandbuf;
	char inputbuf[1920];
	util utils;
	
	utils.set_print_func(ui.top_func);
	utils.print("Console test\n");
	auto input_cb = std::bind(utils.put_char, utils.lock, utils.vt, std::placeholders::_1);
	keyboard kbd(ui.bot_func, input_cb);

	utils.print("Enter Host Name/Addr:");
	hostname = kbd.get_input();
	utils.print("\n");
	
	utils.print("Enter Username:");
	username = kbd.get_input();
	utils.print("Username: " + username + "\n");
	
	utils.print("Enter Password:");
	password = kbd.get_input();
	utils.print("Password: " + password);

	utils.print("Enter port or leave blank to connect to port 22:");
	port = kbd.get_input();
	if(port == "")	port = "22";
	utils.print("Port: " + port);
	
    u32 *SOC_buffer = (u32*)memalign(0x1000, 0x100000);

	if(SOC_buffer == NULL) {
		utils.print("memalign: failed to allocate\n");
		return -1;
	}

	// Now intialise soc:u service
    Result ret = 0;
	if ((ret = socInit(SOC_buffer, 0x100000)) != 0) {
    	utils.print("socInit:" + std::to_string(ret));
        return -1;
	}
	rc = libssh2_init(0);
	if (rc) {
		utils.print("Error: libssh_init()\n");
		return -1;
		//return (EXIT_FAILURE);
	}

	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_ADDRCONFIG;

	if(getaddrinfo(hostname.c_str(), port.c_str(), &hints, &res) != 0)
	{
		utils.print("Host not found\n");
		return -1;
	}

	sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sock == -1) {
		utils.print("socket error:" + std::to_string(errno));
		return -1;
		//return (EXIT_FAILURE);
	}
	if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
		utils.print("Failed to connect");
		//return (EXIT_FAILURE);
		return -1;
	}
	utils.print("Connect successful!\n");
	
	rc = fcntl(sock, F_SETFL, O_NONBLOCK);
	if (rc == -1) {
		return (EXIT_FAILURE);
	}

	utils.print("FCNTL successful\n");
	
	session = libssh2_session_init();
	if (!session) {
		utils.print("SSH init failed");
		return -1;
		//return (EXIT_FAILURE);
	}

	utils.print("Session inited\n");

	rc = libssh2_session_handshake(session, sock);
	if (rc) {
		utils.print("SSH handshake failed");
		return -1;
		//return (EXIT_FAILURE);
	}
	utils.print("Handshaked\n");
	rc = libssh2_userauth_password(session, username.c_str(), password.c_str());
	if (rc) {
		utils.print("Authentication by password failed");
		return -1;
		//return (EXIT_FAILURE);
	} else {
		utils.print("Authentication by password succeeded");
	}

	channel = libssh2_channel_open_session(session);
	if (!channel) {
		utils.print("Unable to open a session");
		return -1;
		//return (EXIT_FAILURE);
	}

	rc = libssh2_channel_request_pty(channel, "vt100");
	if (rc) {
		utils.print("Failed requesting pty\n");
		return -1;
		//return (EXIT_FAILURE);
	}

	rc = libssh2_channel_shell(channel);
	if (rc) {
		utils.print("Unable to request shell on allocated pty\n");
		return -1;
		//return (EXIT_FAILURE);
	}
	utils.print("Channel shell\n");
	libssh2_channel_set_blocking(channel, 0);
	/* Main loop starts here.
	 * In it you will be requested to input a command
	 * command will be executed at remote side
	 * an you will get output from it //*/
	 
	utils.print("Executed till main loop\n");
	std::string buf;
	libssh2_channel_flush(channel);
	fd_set rfds, wfds;
	struct timeval tv;

	do 
	{	
		FD_ZERO(&rfds);
		FD_SET(sock, &rfds);

		tv.tv_sec = 2;
		tv.tv_usec = 30;	
		int retval = select(sock + 1, &rfds, NULL, NULL, &tv);
		printf("retval %d\n", retval);
		if(kbd.has_data() == false)
		{
			if(retval == 1)
			{
				do 
				{
					rc = libssh2_channel_read(channel, inputbuf, 1920);
					buf.append(inputbuf);
					memset(inputbuf, 0, 1920);
               	// printf("RC: ", rc);
				} while (LIBSSH2_ERROR_EAGAIN != rc && rc > 0);
		
				if (rc < 0 && LIBSSH2_ERROR_EAGAIN != rc) 
				{
					utils.print("libssh2_channel_read error");
				}
				
				else
				{
					utils.print(buf);
					buf.clear();
				}
			}
		}
		
		else
		{
		//	utils.print("Output\n");
			commandbuf = kbd.get_input();
			if (commandbuf == EXIT_COMMAND)
				break;
			commandbuf += '\n';
			written = 0;
			do
			{
				rc = libssh2_channel_write(channel, commandbuf.c_str(), commandbuf.length());
				written += rc;
			} while (LIBSSH2_ERROR_EAGAIN != rc && rc > 0 && written != commandbuf.length());

			for(int i = 0; i < commandbuf.size(); i++)
				input_cb('\b');
			commandbuf.clear();
			if (rc < 0 && LIBSSH2_ERROR_EAGAIN != rc) 
			{
				utils.print("libssh2_channel_write error code");
				return -1;
			//  return (EXIT_FAILURE);
			}
		}
	} while (aptMainLoop());
    return 0;
}