#pragma once
#include <cstdint>
#include <cstring>
#include <string>

//Wire format for the server-to-server link ("crosslink").
//
//Two game servers exchange two kinds of datagrams over UDP:
//  VOICE  a raw steam voice packet of a player transmitting on a long-range channel,
//         replayed on the other server through a relay bot
//  MSG    an opaque string (JSON from Lua), e.g. admin chat or announcements
//
//Layout (little endian, as on every platform gmod runs on):
//  0   4  magic "PDXL"
//  4   1  version
//  5   1  type
//  6   2  reserved (0)
//  8   4  instance id of the sending module (drops our own packets unless loopback)
//  12 32  shared secret, zero padded
//  44 ..  body
//
//VOICE body: u64 steamid64, u32 channel key, u8 name length, name, voice data
//MSG body:   message bytes
namespace CrossLink {
	constexpr uint32_t MAGIC = 0x4C584450; //"PDXL"
	constexpr uint8_t VERSION = 1;
	constexpr size_t SECRET_LEN = 32;
	constexpr size_t HEADER_LEN = 44;
	constexpr size_t MAX_NAME = 48;
	constexpr size_t MAX_DATAGRAM = 1400;
	constexpr size_t VOICE_FIXED = 8 + 4 + 1;
	constexpr size_t MAX_MESSAGE = MAX_DATAGRAM - HEADER_LEN;

	enum Type : uint8_t {
		TYPE_VOICE = 1,
		TYPE_MSG = 2,
	};

	struct Header {
		uint8_t type = 0;
		uint32_t instance = 0;
	};

	struct Voice {
		uint64_t steamid = 0;
		uint32_t channel = 0;
		std::string name;
		const char* data = nullptr;
		size_t len = 0;
	};

	inline void PackSecret(const std::string& secret, char out[SECRET_LEN]) {
		std::memset(out, 0, SECRET_LEN);
		std::memcpy(out, secret.data(), secret.size() < SECRET_LEN ? secret.size() : SECRET_LEN);
	}

	//Compares without an early exit so the time taken does not reveal how much matched.
	inline bool SecretEquals(const char* a, const char* b) {
		unsigned char diff = 0;
		for (size_t i = 0; i < SECRET_LEN; i++)
			diff |= (unsigned char)(a[i] ^ b[i]);
		return diff == 0;
	}

	//Writes the header, returns HEADER_LEN.
	inline size_t WriteHeader(char* out, uint8_t type, uint32_t instance, const char secret[SECRET_LEN]) {
		uint32_t magic = MAGIC;
		std::memcpy(out, &magic, 4);
		out[4] = (char)VERSION;
		out[5] = (char)type;
		out[6] = 0;
		out[7] = 0;
		std::memcpy(out + 8, &instance, 4);
		std::memcpy(out + 12, secret, SECRET_LEN);
		return HEADER_LEN;
	}

	//Returns the datagram length, or 0 if it does not fit into maxOut.
	inline size_t BuildVoice(char* out, size_t maxOut, uint32_t instance, const char secret[SECRET_LEN],
	                         uint64_t steamid, uint32_t channel, const std::string& name,
	                         const char* data, size_t len) {
		size_t nameLen = name.size() < MAX_NAME ? name.size() : MAX_NAME;
		size_t total = HEADER_LEN + VOICE_FIXED + nameLen + len;
		if (total > maxOut || total > MAX_DATAGRAM)
			return 0;

		char* p = out + WriteHeader(out, TYPE_VOICE, instance, secret);
		std::memcpy(p, &steamid, 8); p += 8;
		std::memcpy(p, &channel, 4); p += 4;
		*p++ = (char)nameLen;
		std::memcpy(p, name.data(), nameLen); p += nameLen;
		std::memcpy(p, data, len); p += len;
		return (size_t)(p - out);
	}

	inline size_t BuildMessage(char* out, size_t maxOut, uint32_t instance, const char secret[SECRET_LEN],
	                           const char* msg, size_t len) {
		size_t total = HEADER_LEN + len;
		if (len > MAX_MESSAGE || total > maxOut)
			return 0;

		char* p = out + WriteHeader(out, TYPE_MSG, instance, secret);
		std::memcpy(p, msg, len);
		return total;
	}

	//Validates magic, version and secret. On success fills hdr and returns true.
	inline bool ParseHeader(const char* in, size_t len, const char secret[SECRET_LEN], Header& hdr) {
		if (len < HEADER_LEN || len > MAX_DATAGRAM)
			return false;

		uint32_t magic;
		std::memcpy(&magic, in, 4);
		if (magic != MAGIC || (uint8_t)in[4] != VERSION)
			return false;

		if (!SecretEquals(in + 12, secret))
			return false;

		hdr.type = (uint8_t)in[5];
		std::memcpy(&hdr.instance, in + 8, 4);
		return true;
	}

	inline bool ParseVoice(const char* in, size_t len, Voice& v) {
		if (len < HEADER_LEN + VOICE_FIXED)
			return false;

		const char* p = in + HEADER_LEN;
		const char* end = in + len;
		std::memcpy(&v.steamid, p, 8); p += 8;
		std::memcpy(&v.channel, p, 4); p += 4;
		size_t nameLen = (uint8_t)*p++;
		if (nameLen > MAX_NAME || p + nameLen > end)
			return false;

		v.name.assign(p, nameLen); p += nameLen;
		v.data = p;
		v.len = (size_t)(end - p);
		return v.len > 0;
	}
}
