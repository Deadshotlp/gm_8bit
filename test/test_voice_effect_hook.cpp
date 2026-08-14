// Verifies the stack discipline of VoiceEffect::Run against a mock Lua stack.
// The original code leaked one Pop on the PCall error path, which pops below the caller's
// stack base and corrupts whatever Lua was executing. These tests pin that down.
//
// Build & run: see test/run.sh
#include "../source/voice_effect_hook.h"

#include <cassert>
#include <cmath>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

using namespace GarrysMod::Lua;

namespace {

struct Value {
	int type = Type::Nil;
	double num = 0;
	std::string str;
	// Table contents, integer keys only (all this code uses).
	std::map<int, Value>* tbl = nullptr;
};

Value MakeNumber(double d) { Value v; v.type = Type::Number; v.num = d; return v; }

class MockLua : public ILuaBase {
public:
	std::vector<Value> stack;
	std::map<int, Value>* sampleTable;
	// Scenario knobs
	bool hookRunExists = true;
	bool pcallFails = false;
	bool returnTable = true;
	bool errorObjectIsString = true;
	// Set by the test to rewrite what the "Lua hook" hands back.
	std::map<int, Value> returned;
	int minDepthSeen = 0;

	MockLua() { sampleTable = new std::map<int, Value>(); }
	~MockLua() { delete sampleTable; }

	void Track() { if ((int)stack.size() < minDepthSeen) minDepthSeen = (int)stack.size(); }

	int Top() override { return (int)stack.size(); }

	void Pop(int iAmt = 1) override {
		for (int i = 0; i < iAmt; i++) {
			// This is the assertion that matters: the original code popped past its base.
			assert(!stack.empty() && "Lua stack underflow - popped below the caller's base");
			stack.pop_back();
		}
		Track();
	}

	void PushSpecial(int) override { Value v; v.type = Type::Table; v.tbl = nullptr; stack.push_back(v); }

	void GetField(int, const char* name) override {
		Value v;
		if (std::string(name) == "hook") {
			v.type = Type::Table;
		} else if (std::string(name) == "Run") {
			v.type = hookRunExists ? Type::Function : Type::Nil;
		}
		stack.push_back(v);
	}

	void PushString(const char* s, unsigned int = 0) override {
		Value v; v.type = Type::String; v.str = s ? s : ""; stack.push_back(v);
	}
	void PushNumber(double d) override { stack.push_back(MakeNumber(d)); }
	void PushNil() override { stack.push_back(Value()); }
	void CreateTable() override { Value v; v.type = Type::Table; v.tbl = new std::map<int, Value>(); stack.push_back(v); }

	void ReferencePush(int) override {
		Value v; v.type = Type::Table; v.tbl = sampleTable; stack.push_back(v);
	}

	Value& At(int pos) {
		int idx = pos < 0 ? (int)stack.size() + pos : pos - 1;
		assert(idx >= 0 && idx < (int)stack.size() && "stack index out of range");
		return stack[idx];
	}

	// Note: lua_rawset/lua_rawget resolve the table index while the key (and value) are
	// still on the stack, so the lookup has to happen before popping them.
	void RawSet(int pos) override {
		Value& t = At(pos);
		assert(t.type == Type::Table && t.tbl);
		std::map<int, Value>* tbl = t.tbl;

		Value val = stack.back(); stack.pop_back();
		Value key = stack.back(); stack.pop_back();
		if (val.type == Type::Nil) tbl->erase((int)key.num);
		else (*tbl)[(int)key.num] = val;
		Track();
	}
	void SetTable(int pos) override { RawSet(pos); }

	void RawGet(int pos) override {
		Value& t = At(pos);
		assert(t.type == Type::Table && t.tbl);
		std::map<int, Value>* tbl = t.tbl;

		Value key = stack.back(); stack.pop_back();
		auto it = tbl->find((int)key.num);
		stack.push_back(it == tbl->end() ? Value() : it->second);
		Track();
	}
	void GetTable(int pos) override { RawGet(pos); }

	int PCall(int iArgs, int iResults, int) override {
		// Real semantics: pop the function plus its args, then push either the results or
		// the error object.
		for (int i = 0; i < iArgs + 1; i++) { assert(!stack.empty()); stack.pop_back(); }
		Track();

		if (pcallFails) {
			Value err;
			if (errorObjectIsString) { err.type = Type::String; err.str = "addon blew up"; }
			else { err.type = Type::Table; err.tbl = nullptr; }
			stack.push_back(err);
			return 1;
		}

		for (int i = 0; i < iResults; i++) {
			Value v;
			if (returnTable) { v.type = Type::Table; v.tbl = &returned; }
			stack.push_back(v);
		}
		return 0;
	}

	bool IsType(int pos, int type) override { return At(pos).type == type; }
	int GetType(int pos) override { return At(pos).type; }
	double GetNumber(int pos = -1) override { return At(pos).num; }
	const char* GetString(int pos = -1, unsigned int* = nullptr) override {
		Value& v = At(pos);
		return v.type == Type::String ? v.str.c_str() : nullptr;
	}

#include "_stubs.inc"
};

int g_failures = 0;

void Check(bool cond, const char* what) {
	std::printf("  %s %s\n", cond ? "PASS" : "FAIL", what);
	if (!cond) g_failures++;
}

// Runs one scenario with `pre` junk already sitting on the stack, mimicking the fact that
// the detour fires while Lua has its own frames live.
struct Result {
	int depthBefore, depthAfter, minDepth;
	bool ok;
	std::string err;
	std::vector<int16_t> pcm;
};

Result RunScenario(MockLua& lua, std::vector<int16_t> pcm, int& fill) {
	// Pretend the engine's Lua stack already holds three live values.
	for (int i = 0; i < 3; i++) lua.PushNumber(1000 + i);
	lua.minDepthSeen = (int)lua.stack.size();

	Result r;
	r.depthBefore = lua.Top();
	std::string err;
	r.ok = VoiceEffect::Run(&lua, 1, fill, 42, pcm.data(), (int)pcm.size(), err);
	r.err = err;
	r.depthAfter = lua.Top();
	r.minDepth = lua.minDepthSeen;
	r.pcm = pcm;
	return r;
}

} // namespace

int main() {
	int failures = 0;

	{
		std::printf("happy path: hook returns modified samples\n");
		MockLua lua;
		lua.returned[1] = MakeNumber(-100);
		lua.returned[2] = MakeNumber(200);
		lua.returned[3] = MakeNumber(300);
		int fill = 0;
		auto r = RunScenario(lua, {1, 2, 3}, fill);
		Check(r.ok, "no error reported");
		Check(r.depthAfter == r.depthBefore, "stack restored to entry depth");
		Check(r.minDepth >= 3, "never popped below the caller's frames");
		Check(r.pcm[0] == -100 && r.pcm[1] == 200 && r.pcm[2] == 300, "samples rewritten");
		Check(fill == 3, "fill count tracked");
		// The module pushed the input samples into the shared table.
		Check((*lua.sampleTable)[1].num == 1 && (*lua.sampleTable)[3].num == 3, "input marshalled");
	}

	{
		std::printf("error path: hook raises (the original underflow bug)\n");
		MockLua lua;
		lua.pcallFails = true;
		int fill = 0;
		auto r = RunScenario(lua, {7, 8, 9}, fill);
		Check(!r.ok, "error reported to caller");
		Check(r.err == "addon blew up", "error message propagated");
		Check(r.depthAfter == r.depthBefore, "stack restored to entry depth");
		Check(r.minDepth >= 3, "never popped below the caller's frames");
		Check(r.pcm[0] == 7 && r.pcm[1] == 8 && r.pcm[2] == 9, "samples left untouched on error");
	}

	{
		std::printf("error path: error object is not a string\n");
		MockLua lua;
		lua.pcallFails = true;
		lua.errorObjectIsString = false;
		int fill = 0;
		auto r = RunScenario(lua, {1, 2}, fill);
		Check(!r.ok, "error reported");
		Check(r.err == "(non-string error)", "null GetString handled");
		Check(r.depthAfter == r.depthBefore, "stack restored to entry depth");
	}

	{
		std::printf("hook.Run missing (state teardown)\n");
		MockLua lua;
		lua.hookRunExists = false;
		int fill = 0;
		auto r = RunScenario(lua, {1, 2}, fill);
		Check(r.ok, "treated as no-op");
		Check(r.depthAfter == r.depthBefore, "stack restored to entry depth");
		Check(r.minDepth >= 3, "never popped below the caller's frames");
	}

	{
		std::printf("hook returns a non-table\n");
		MockLua lua;
		lua.returnTable = false;
		int fill = 0;
		auto r = RunScenario(lua, {5, 6}, fill);
		Check(r.ok, "no error");
		Check(r.depthAfter == r.depthBefore, "stack restored to entry depth");
		Check(r.pcm[0] == 5 && r.pcm[1] == 6, "samples left untouched");
	}

	{
		std::printf("out-of-range and NaN values are clamped, not cast as UB\n");
		MockLua lua;
		lua.returned[1] = MakeNumber(1e9);
		lua.returned[2] = MakeNumber(-1e9);
		lua.returned[3] = MakeNumber(std::nan(""));
		lua.returned[4] = MakeNumber(32767.0);
		int fill = 0;
		auto r = RunScenario(lua, {0, 0, 0, 0}, fill);
		Check(r.pcm[0] == 32767, "+1e9 clamps to INT16_MAX");
		Check(r.pcm[1] == -32768, "-1e9 clamps to INT16_MIN");
		Check(r.pcm[2] == -32768, "NaN clamps to INT16_MIN");
		Check(r.pcm[3] == 32767, "in-range value preserved");
	}

	{
		std::printf("shorter packet clears the previous packet's tail\n");
		MockLua lua;
		int fill = 0;
		auto r1 = RunScenario(lua, {11, 22, 33, 44, 55}, fill);
		(void)r1;
		Check(fill == 5, "first packet recorded 5 samples");
		Check(lua.sampleTable->size() == 5, "table holds 5 entries");

		auto r2 = RunScenario(lua, {99, 98}, fill);
		Check(r2.depthAfter == r2.depthBefore, "stack restored to entry depth");
		Check(fill == 2, "second packet recorded 2 samples");
		Check(lua.sampleTable->size() == 2, "stale tail cleared - no cross-packet leak");
		Check((*lua.sampleTable)[1].num == 99, "new samples present");
	}

	{
		std::printf("zero samples\n");
		MockLua lua;
		int fill = 0;
		auto r = RunScenario(lua, {}, fill);
		Check(r.ok, "no error");
		Check(r.depthAfter == r.depthBefore, "stack restored to entry depth");
		Check(r.minDepth >= 3, "never popped below the caller's frames");
	}

	{
		std::printf("null lua state is a no-op\n");
		int fill = 0;
		std::string err;
		int16_t pcm[2] = {1, 2};
		bool ok = VoiceEffect::Run(nullptr, 1, fill, 42, pcm, 2, err);
		Check(ok, "returns cleanly");
		Check(pcm[0] == 1 && pcm[1] == 2, "samples untouched");
	}

	failures = g_failures;
	std::printf("\n%s (%d failing checks)\n", failures ? "FAILED" : "ALL CHECKS PASSED", failures);
	return failures ? 1 : 0;
}
