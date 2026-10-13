#pragma once
#include <cstdint>
#include <cstddef>
#include <string>

class Net {
public:
	Net();
	~Net();
	void SendPacket(const char* dest, uint16_t port, const char* buffer, uint32_t len);

	//Receiving socket for the crosslink. Bind opens it non-blocking on all interfaces.
	bool Bind(uint16_t port, std::string& outError);
	void Unbind();
	bool IsBound() const { return m_recvSocket >= 0; }

	//Reads one datagram without blocking. Returns its length, 0 if nothing is waiting
	//and -1 on error. fromIp receives the sender address in dotted form.
	int Receive(char* buffer, size_t maxLen, std::string& fromIp);

private:
	int m_socket;
	int m_recvSocket = -1;
};
