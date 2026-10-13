#pragma once
#include <string>
#include <unordered_map>
#include <cstdint>
#include <chrono>
#include "crosslink.h"

//A player whose voice is relayed to the partner server.
struct RelayEntry {
	uint32_t channel = 0;
	std::string name;
};

struct CrossLinkState {
	std::string peerIp;
	uint16_t peerPort = 0;
	std::string allowFrom; //empty = accept from any address that knows the secret
	char secret[CrossLink::SECRET_LEN] = {0};
	bool hasSecret = false;
	bool loopback = false; //accept our own packets (single-server testing)
	uint32_t instance = 0;

	//userid -> relayed channel
	std::unordered_map<int, RelayEntry> relay;
	//remote steamid64 -> userid of the local relay bot that replays it
	std::unordered_map<uint64_t, int> bots;
	//remote steamid64 -> last time Lua was told about this speaker
	std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> lastNotify;

	uint64_t sentVoice = 0, sentMsg = 0, recvVoice = 0, recvMsg = 0, dropped = 0;
};

struct EightbitState {
	bool broadcastPackets = false;
	uint16_t sample_rate = 24000;
	uint16_t port = 4000;
	std::string ip = "127.0.0.1";
	std::unordered_map<int, std::tuple<IVoiceCodec*, int>> afflictedPlayers;
	CrossLinkState link;
};
