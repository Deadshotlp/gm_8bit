#include "net.h"
#include <cstdint>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#endif

#ifdef __linux
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#endif

Net::Net() {
#ifdef _WIN32
	WSADATA wsaData;
	int res = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (res != NO_ERROR)
		throw "Initialization Error!";

	m_socket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_socket == INVALID_SOCKET)
		throw "Invalid socket!";
#elif __linux
	m_socket = socket(AF_INET, SOCK_DGRAM, 0);
	if (m_socket < 0)
		throw "Invalid socket!";
#endif
}

void Net::SendPacket(const char* dest, uint16_t port, const char* buffer, uint32_t len) {
	sockaddr_in dest_str;
	dest_str.sin_family = AF_INET;
	dest_str.sin_port = htons(port);
	inet_pton(AF_INET, dest, &dest_str.sin_addr);
	sendto(m_socket, buffer, len, 0, (sockaddr*)&dest_str, sizeof(dest_str));
}

bool Net::Bind(uint16_t port, std::string& outError) {
	Unbind();

	int s = (int)socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s < 0) {
		outError = "socket() failed";
		return false;
	}

	sockaddr_in addr;
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);

	if (bind(s, (sockaddr*)&addr, sizeof(addr)) != 0) {
		outError = "bind() failed - port in use or not allocated";
#ifdef _WIN32
		closesocket(s);
#else
		close(s);
#endif
		return false;
	}

	//Never block the game thread: Receive is polled from Lua every tick.
#ifdef _WIN32
	u_long nonBlocking = 1;
	ioctlsocket(s, FIONBIO, &nonBlocking);
#else
	int flags = fcntl(s, F_GETFL, 0);
	fcntl(s, F_SETFL, flags | O_NONBLOCK);
#endif

	m_recvSocket = s;
	return true;
}

void Net::Unbind() {
	if (m_recvSocket < 0)
		return;
#ifdef _WIN32
	closesocket(m_recvSocket);
#else
	close(m_recvSocket);
#endif
	m_recvSocket = -1;
}

int Net::Receive(char* buffer, size_t maxLen, std::string& fromIp) {
	if (m_recvSocket < 0)
		return -1;

	sockaddr_in from;
	socklen_t fromLen = sizeof(from);
	int n = (int)recvfrom(m_recvSocket, buffer, (int)maxLen, 0, (sockaddr*)&from, &fromLen);
	if (n < 0) {
#ifdef _WIN32
		int err = WSAGetLastError();
		//An ICMP "port unreachable" from an earlier send surfaces here on Windows; it is
		//not fatal for a datagram socket.
		if (err == WSAEWOULDBLOCK || err == WSAECONNRESET || err == WSAEMSGSIZE)
			return 0;
#else
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR || errno == ECONNREFUSED)
			return 0;
#endif
		return -1;
	}

	char ip[INET_ADDRSTRLEN] = {0};
	inet_ntop(AF_INET, &from.sin_addr, ip, sizeof(ip));
	fromIp = ip;
	return n;
}

Net::~Net() {
	Unbind();
#ifdef _WIN32
	closesocket(m_socket);
	WSACleanup();
#elif __linux
	close(m_socket);
#endif
}
