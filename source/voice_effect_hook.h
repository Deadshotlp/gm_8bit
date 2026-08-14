#pragma once
#include <GarrysMod/Lua/LuaBase.h>
#include <cstdint>
#include <string>

namespace VoiceEffect {

	//Runs the "ApplyVoiceEffect" hook over the decoded PCM in place.
	//
	//tableRef is a registry reference to a table we reuse for every packet - allocating a
	//fresh table per voice packet (20-40 per second per speaking player) is what turned
	//this into a denial-of-service vector. tableFill carries how many indices the previous
	//call populated so the tail can be cleared instead of leaking another player's samples
	//into a script that ignores the count argument.
	//
	//Every exit path restores the Lua stack to exactly the depth it started at.
	//Returns false if the hook raised an error, with the message in outError.
	inline bool Run(GarrysMod::Lua::ILuaBase* lua, int tableRef, int& tableFill,
	                int uid, int16_t* pcm, int samples, std::string& outError) {
		if (lua == nullptr || tableRef == -1)
			return true;

		const int stackBase = lua->Top();

		lua->PushSpecial(GarrysMod::Lua::SPECIAL_GLOB);
		lua->GetField(-1, "hook");
		lua->GetField(-1, "Run");

		//hook or hook.Run missing - happens during state teardown. Nothing to do.
		if (!lua->IsType(-1, GarrysMod::Lua::Type::Function)) {
			lua->Pop(lua->Top() - stackBase);
			return true;
		}

		lua->PushString("ApplyVoiceEffect");
		lua->PushNumber(uid);

		lua->ReferencePush(tableRef);
		for (int i = 0; i < samples; i++) {
			lua->PushNumber(i + 1);
			lua->PushNumber(pcm[i]);
			lua->RawSet(-3);
		}
		//Clear whatever a longer previous packet left behind.
		for (int i = samples; i < tableFill; i++) {
			lua->PushNumber(i + 1);
			lua->PushNil();
			lua->RawSet(-3);
		}
		tableFill = samples;

		lua->PushNumber(samples);

		if (lua->PCall(4, 1, 0) != 0) {
			const char* err = lua->GetString(-1);
			outError = err ? err : "(non-string error)";
			lua->Pop(lua->Top() - stackBase);
			return false;
		}

		if (lua->IsType(-1, GarrysMod::Lua::Type::Table)) {
			for (int i = 0; i < samples; i++) {
				lua->PushNumber(i + 1);
				//Raw access on purpose: a returned table with an __index metamethod must not
				//be able to re-enter Lua from inside this loop.
				lua->RawGet(-2);

				if (lua->IsType(-1, GarrysMod::Lua::Type::Number)) {
					double v = lua->GetNumber(-1);
					//Clamp before the cast - converting an out-of-range or NaN double to
					//int16_t is undefined behaviour. The inverted compare catches NaN too.
					if (!(v >= -32768.0)) v = -32768.0;
					else if (v > 32767.0) v = 32767.0;

					pcm[i] = (int16_t)v;
				}

				lua->Pop();
			}
		}

		lua->Pop(lua->Top() - stackBase);
		return true;
	}
}
