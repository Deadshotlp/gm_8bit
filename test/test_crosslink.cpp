//Round-trip and rejection tests for the crosslink wire format (source/crosslink.h).
#include "../source/crosslink.h"

#include <cassert>
#include <cstdio>
#include <string>

using namespace CrossLink;

int main() {
	char secret[SECRET_LEN], other[SECRET_LEN];
	PackSecret("geheim-123456", secret);
	PackSecret("falsch-123456", other);

	char buf[MAX_DATAGRAM];
	const char voice[] = "12345678\x0b\xc0\x5d\x06\x03\x00abcCRC!";
	size_t n = BuildVoice(buf, sizeof(buf), 42, secret, 76561198000000001ull, 7, "CT-7567 Rex", voice, sizeof(voice));
	assert(n == HEADER_LEN + VOICE_FIXED + 11 + sizeof(voice));

	Header h;
	assert(ParseHeader(buf, n, secret, h));
	assert(h.type == TYPE_VOICE && h.instance == 42);
	Voice v;
	assert(ParseVoice(buf, n, v));
	assert(v.steamid == 76561198000000001ull && v.channel == 7 && v.name == "CT-7567 Rex");
	assert(v.len == sizeof(voice) && std::string(v.data, v.len) == std::string(voice, sizeof(voice)));

	//wrong secret, truncated, corrupted magic, bad name length
	assert(!ParseHeader(buf, n, other, h));
	assert(!ParseHeader(buf, HEADER_LEN - 1, secret, h));
	char bad[MAX_DATAGRAM];
	std::memcpy(bad, buf, n); bad[0] ^= 1;
	assert(!ParseHeader(bad, n, secret, h));
	std::memcpy(bad, buf, n); bad[HEADER_LEN + 12] = (char)200;
	assert(!ParseVoice(bad, n, v));
	assert(!ParseVoice(buf, HEADER_LEN + VOICE_FIXED, v));

	//oversized payloads are refused instead of truncated
	static char big[MAX_DATAGRAM];
	assert(BuildVoice(buf, sizeof(buf), 1, secret, 1, 1, "x", big, sizeof(big)) == 0);
	assert(BuildMessage(buf, sizeof(buf), 1, secret, big, MAX_MESSAGE + 1) == 0);

	//names are clipped to MAX_NAME
	std::string longName(100, 'n');
	n = BuildVoice(buf, sizeof(buf), 1, secret, 1, 1, longName, voice, sizeof(voice));
	assert(ParseVoice(buf, n, v) && v.name.size() == MAX_NAME);

	const char msg[] = "{\"t\":\"admin\",\"text\":\"hallo\"}";
	n = BuildMessage(buf, sizeof(buf), 9, secret, msg, sizeof(msg) - 1);
	assert(ParseHeader(buf, n, secret, h) && h.type == TYPE_MSG && h.instance == 9);
	assert(std::string(buf + HEADER_LEN, n - HEADER_LEN) == msg);

	std::puts("crosslink: all tests passed");
	return 0;
}
