#include "pch.h"
#include "LuaGc.h"

extern "C" {
#include "lua.h"
#include "lauxlib.h"
}

#include "core/globals/Globals.h"
#include "core/memory/Memory.h"
#include "core/roblox/classes/Classes.h"
#include "core/roblox/offsets/Offsets.h"

#include <cstring>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <functional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace Cheat {
namespace Features {
namespace LuaGc {
namespace {

bool LooksLikeLuaState(std::uint64_t L)
{
	if (!g_Memory.IsValid(L) || (L & 0xF))
		return false;

	const std::uint64_t top = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Top);
	const std::uint64_t base = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Base);
	const std::uint64_t G = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Global);

	if (!g_Memory.IsValid(top) || !g_Memory.IsValid(base) || !g_Memory.IsValid(G))
		return false;

	if ((top | base) & 0xF)
		return false;

	if (top < base || (top - base) > 0x40000)
		return false;

	const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
	const std::uint8_t st = g_Memory.Read<std::uint8_t>(G + Offsets::LuauGlobal::gcstate);

	if (tb < 1024 || tb > (512ull << 20))
		return false;

	if (st > 4)
		return false;

	return true;
}

std::uint64_t FindScriptContext()
{
	if (!g_Memory.IsAttached())
		return 0;

	const auto& dm = Globals::InstanceDataModel;
	if (!g_Memory.IsValid(dm.address))
		return 0;

	for (const auto& c : dm.GetChildren())
	{
		if (!g_Memory.IsValid(c.address))
			continue;

		if (c.GetClassName() == "ScriptContext")
			return c.address;
	}

	return 0;
}

struct GameVm
{
	std::uint64_t L = 0;
	std::uint64_t G = 0;
	std::uint64_t tb = 0;
	std::uint32_t wrap = 0;
};

void CollectGameVms(std::vector<GameVm>& out)
{
	out.clear();
	const std::uint64_t sc = FindScriptContext();
	if (!sc)
		return;

	const uintptr_t wraps[3] = {
		Offsets::ScriptContext::VmWrapper,
		Offsets::ScriptContext::VmWrapperBig,
		Offsets::ScriptContext::VmWrapper2,
	};
	const uintptr_t loffs[2] = {
		Offsets::ScriptContext::LuaState,
		Offsets::ScriptContext::LuaStateAlt,
	};

	std::unordered_set<std::uint64_t> seen_g;

	for (int wi = 0; wi < 3; ++wi)
	{
		const std::uint64_t w = g_Memory.Read<std::uint64_t>(sc + wraps[wi]);
		if (!g_Memory.IsValid(w))
			continue;

		for (int li = 0; li < 2; ++li)
		{
			const std::uint64_t L = g_Memory.Read<std::uint64_t>(w + loffs[li]);
			if (!LooksLikeLuaState(L))
				continue;

			const std::uint64_t G = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Global);
			if (!g_Memory.IsValid(G))
				continue;

			if (!seen_g.insert(G).second)
				continue;

			GameVm vm{};
			vm.L = L;
			vm.G = G;
			vm.tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
			vm.wrap = (std::uint32_t)wraps[wi];
			out.push_back(vm);
		}
	}
}

// SC wraps -> L с большим totalbytes (setgc)
std::uint64_t ResolveGameLuaState()
{
	std::vector<GameVm> vms;
	CollectGameVms(vms);

	std::uint64_t best = 0;
	std::uint64_t best_tb = 0;
	for (const auto& vm : vms)
	{
		if (vm.tb > best_tb)
		{
			best_tb = vm.tb;
			best = vm.L;
		}
	}
	return best;
}

std::uint64_t GameGlobal()
{
	const std::uint64_t L = ResolveGameLuaState();
	if (!L)
		return 0;

	const std::uint64_t G = g_Memory.Read<std::uint64_t>(L + Offsets::LuaState::Global);
	if (!g_Memory.IsValid(G))
		return 0;

	return G;
}

int l_setgc(lua_State* L)
{
	const char* opt = luaL_checkstring(L, 1);
	if (!opt)
		return 0;

	const std::uint64_t G = GameGlobal();
	if (!G)
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	// stop
	if (std::strcmp(opt, "stop") == 0)
	{
		const std::int64_t m1 = -1;
		g_Memory.Write<std::int64_t>(G + Offsets::LuauGlobal::GCthreshold, m1);
		lua_pushboolean(L, 1);
		return 1;
	}

	// restart
	if (std::strcmp(opt, "restart") == 0)
	{
		const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
		g_Memory.Write<std::uint64_t>(G + Offsets::LuauGlobal::GCthreshold, tb);
		lua_pushboolean(L, 1);
		return 1;
	}

	// count -> KB как lua_gc COUNT
	if (std::strcmp(opt, "count") == 0)
	{
		const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
		lua_pushnumber(L, static_cast<lua_Number>(tb >> 10));
		return 1;
	}

	// isrunning
	if (std::strcmp(opt, "isrunning") == 0)
	{
		const std::int64_t thr = g_Memory.Read<std::int64_t>(G + Offsets::LuauGlobal::GCthreshold);
		lua_pushboolean(L, thr != -1);
		return 1;
	}

	// pause
	if (std::strcmp(opt, "pause") == 0 || std::strcmp(opt, "setpause") == 0)
	{
		const int old = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcpause);
		if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
		{
			int v = static_cast<int>(luaL_checkinteger(L, 2));
			if (v < 0) v = 0;
			if (v > 1000) v = 1000;
			g_Memory.Write<std::int32_t>(G + Offsets::LuauGlobal::gcpause, v);
		}

		lua_pushinteger(L, old);
		return 1;
	}

	// stepmul
	if (std::strcmp(opt, "stepmul") == 0 || std::strcmp(opt, "setstepmul") == 0)
	{
		const int old = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcstepmul);
		if (lua_gettop(L) >= 2 && !lua_isnil(L, 2))
		{
			int v = static_cast<int>(luaL_checkinteger(L, 2));
			if (v < 0) v = 0;
			if (v > 10000) v = 10000;
			g_Memory.Write<std::int32_t>(G + Offsets::LuauGlobal::gcstepmul, v);
		}

		lua_pushinteger(L, old);
		return 1;
	}

	// collect/step — только через lua_gc в процессе, снаружи нет
	if (std::strcmp(opt, "collect") == 0 || std::strcmp(opt, "step") == 0)
	{
		lua_pushnil(L);
		lua_pushstring(L, "collect/step need ingame call");
		return 2;
	}

	lua_pushnil(L);
	lua_pushstring(L, "unknown setgc opt");
	return 2;
}

const char* TtName(int tt)
{
	if (tt == 0) return "nil";
	if (tt == 1) return "bool";
	if (tt == 2) return "lightud";
	if (tt == 3) return "number";
	if (tt == 4) return "vector?";
	if (tt == 5) return "vector";
	if (tt == 6) return "string";
	if (tt == 7) return "table";
	if (tt == 8) return "fn?";
	if (tt == 9) return "userdata";
	if (tt == 10) return "thread";
	if (tt == 11) return "buffer";
	return "other";
}

std::uint64_t GcoNext(std::uint64_t obj, int tt)
{
	// gclist: table @+40, остальное чаще @+8 (propagatemark)
	if (tt == 7)
		return g_Memory.Read<std::uint64_t>(obj + 40);

	return g_Memory.Read<std::uint64_t>(obj + 8);
}

int WalkGcoList(std::uint64_t head, int* by_tt, int by_n, int& total, std::uint64_t* samples, int* sample_tt, int& nsample, int sample_cap)
{
	int n = 0;
	std::uint64_t cur = head;
	for (int i = 0; i < 80000; ++i)
	{
		if (!g_Memory.IsValid(cur) || (cur & 0x7))
			break;

		const int tt = (int)g_Memory.Read<std::uint8_t>(cur + 1);
		if (tt < 0 || tt >= by_n)
			break;

		++by_tt[tt];
		++total;
		++n;

		if (nsample < sample_cap)
		{
			samples[nsample] = cur;
			sample_tt[nsample] = tt;
			++nsample;
		}

		const std::uint64_t nxt = GcoNext(cur, tt);
		if (nxt == cur)
			break;
		cur = nxt;
	}
	return n;
}

int CountPageChain(std::uint64_t head, std::uint64_t sentinel, int lim, int next_off)
{
	int n = 0;
	std::uint64_t p = head;
	for (int i = 0; i < lim; ++i)
	{
		if (!p || p == sentinel)
			break;
		if (!g_Memory.IsValid(p))
			break;
		++n;
		const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(p + (std::uint64_t)next_off);
		if (nxt == p)
			break;
		p = nxt;
	}
	return n;
}

int CountPages(std::uint64_t G)
{
	const std::uint64_t end = G + Offsets::LuauGlobal::gcopages_end;
	int n = CountPageChain(
		g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages),
		end, 200000, (int)Offsets::LuauGlobal::page_next_free);

	// all pages — next @+8, тут и полные page после unlink с freelist
	n += CountPageChain(
		g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages_large),
		0, 200000, (int)Offsets::LuauGlobal::page_next_all);

	for (int sc = 0; sc < 48; ++sc)
	{
		const std::uint64_t head = g_Memory.Read<std::uint64_t>(
			G + Offsets::LuauGlobal::gcopages_sizeclass + (std::uint64_t)sc * 8ull);
		n += CountPageChain(head, 0, 20000, (int)Offsets::LuauGlobal::page_next_free);
	}

	return n;
}

// getgc_info() — старый дамп stats
int l_getgc_info(lua_State* L)
{
	const std::uint64_t gameL = ResolveGameLuaState();
	const std::uint64_t G = GameGlobal();
	if (!G || !gameL)
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	int by_tt[16] = {};
	int total = 0;
	std::uint64_t samples[24] = {};
	int sample_tt[24] = {};
	int nsample = 0;

	const std::uint64_t gray = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gray);
	const std::uint64_t grayagain = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::grayagain);
	const std::uint64_t weak = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::weak);

	const int n_gray = WalkGcoList(gray, by_tt, 16, total, samples, sample_tt, nsample, 24);
	const int n_ga = WalkGcoList(grayagain, by_tt, 16, total, samples, sample_tt, nsample, 24);
	const int n_weak = WalkGcoList(weak, by_tt, 16, total, samples, sample_tt, nsample, 24);
	const int pages = CountPages(G);

	const std::uint64_t tb = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::totalbytes);
	const std::int64_t thr = g_Memory.Read<std::int64_t>(G + Offsets::LuauGlobal::GCthreshold);
	const int st = (int)g_Memory.Read<std::uint8_t>(G + Offsets::LuauGlobal::gcstate);
	const int pause = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcpause);
	const int mul = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::gcstepmul);

	lua_createtable(L, 0, 16);

	lua_pushnumber(L, (lua_Number)(tb >> 10));
	lua_setfield(L, -2, "kb");

	lua_pushinteger(L, st);
	lua_setfield(L, -2, "gcstate");

	lua_pushboolean(L, thr != -1);
	lua_setfield(L, -2, "isrunning");

	lua_pushinteger(L, pause);
	lua_setfield(L, -2, "pause");

	lua_pushinteger(L, mul);
	lua_setfield(L, -2, "stepmul");

	lua_pushinteger(L, pages);
	lua_setfield(L, -2, "pages");

	lua_pushinteger(L, n_gray);
	lua_setfield(L, -2, "gray");

	lua_pushinteger(L, n_ga);
	lua_setfield(L, -2, "grayagain");

	lua_pushinteger(L, n_weak);
	lua_setfield(L, -2, "weak");

	lua_pushinteger(L, total);
	lua_setfield(L, -2, "total");

	char buf[32];
	std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)gameL);
	lua_pushstring(L, buf);
	lua_setfield(L, -2, "L");

	std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)G);
	lua_pushstring(L, buf);
	lua_setfield(L, -2, "G");

	lua_createtable(L, 0, 16);
	for (int t = 0; t < 16; ++t)
	{
		if (by_tt[t] <= 0)
			continue;
		lua_pushinteger(L, by_tt[t]);
		lua_setfield(L, -2, TtName(t));
	}
	lua_setfield(L, -2, "by_type");

	lua_createtable(L, nsample, 0);
	for (int i = 0; i < nsample; ++i)
	{
		lua_createtable(L, 0, 3);
		lua_pushinteger(L, sample_tt[i]);
		lua_setfield(L, -2, "tt");
		lua_pushstring(L, TtName(sample_tt[i]));
		lua_setfield(L, -2, "name");
		std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)samples[i]);
		lua_pushstring(L, buf);
		lua_setfield(L, -2, "addr");
		lua_rawseti(L, -2, i + 1);
	}
	lua_setfield(L, -2, "samples");

	return 1;
}

// ---- game table proxy (getgc true) ----

// findgc("_tag", "xxx") — не плодим 40k проксей
const char* g_fkey = nullptr;
const char* g_fval = nullptr;
// findgc(nil, "jewsploit_ammo_test") / режим value-only
bool g_fval_only = false;
std::uint64_t g_cached_find = 0;
// interned TString* из strt — быстрее чем memcmp каждого ключа
std::uint64_t g_fkey_ts = 0;
std::uint64_t g_fval_ts = 0;
// TString.hash @+16 (наш layout: next@+8 len@+20 data@+24)
unsigned int g_fkey_hash = 0;

// LooksLikeTable -> TableFindStr на том же obj
std::uint64_t g_hdr_tbl = 0;
std::uint8_t g_hdr_lsz = 0;
std::uint64_t g_hdr_node = 0;

// proxy write: tbl+keyhash -> node
std::unordered_map<std::uint64_t, std::uint64_t> g_pc;

unsigned int LuauStrHash(const char* str, std::size_t len);

std::uint64_t PcId(std::uint64_t tbl, const char* key, std::size_t klen)
{
	return tbl ^ ((std::uint64_t)LuauStrHash(key, klen) * 0x9E3779B97F4A7C15ull) ^ ((std::uint64_t)klen << 17);
}

bool TsEq(std::uint64_t ts, const char* key, std::size_t klen)
{
	if (!g_Memory.IsValid(ts) || (ts & 7))
		return false;

	if (g_Memory.Read<std::uint8_t>(ts + 1) != 6)
		return false;

	const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
	if (len != (std::uint32_t)klen || len > 512)
		return false;

	char buf[512];
	if (g_Memory.ReadRaw((uintptr_t)(ts + 24), buf, len) != len)
		return false;

	return std::memcmp(buf, key, klen) == 0;
}

// luau short hash (BytecodeBuilder::getStringHash / luaS_hash len<32)
unsigned int LuauStrHash(const char* str, std::size_t len)
{
	unsigned int h = (unsigned int)len;
	for (std::size_t i = len; i > 0; --i)
		h ^= (h << 5) + (h >> 2) + (unsigned char)str[i - 1];
	return h;
}

std::uint64_t StrtChainFind(
	std::uint64_t hash_base,
	int bucket,
	const char* needle,
	std::size_t nlen)
{
	std::uint64_t ts = g_Memory.Read<std::uint64_t>(
		hash_base + (std::uint64_t)bucket * 8ull);

	for (int k = 0; k < 100000 && ts; ++k)
	{
		if (!g_Memory.IsValid(ts) || (ts & 7))
			break;

		if (TsEq(ts, needle, nlen))
			return ts;

		const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(ts + 8);
		if (nxt == ts)
			break;
		ts = nxt;
	}

	return 0;
}

std::uint64_t FindStrtFirst(std::uint64_t G, const char* needle)
{
	if (!needle || !G)
		return 0;

	const int size = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::strt_size);
	const std::uint64_t hash = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::strt_hash);
	if (size <= 0 || size > (1 << 22) || !g_Memory.IsValid(hash))
		return 0;

	const std::size_t nlen = std::strlen(needle);

	// сначала один bucket — miss = полный скан (seed/longstr хз)
	if ((size & (size - 1)) == 0)
	{
		const int buck = (int)(LuauStrHash(needle, nlen) & (unsigned int)(size - 1));
		const std::uint64_t hit = StrtChainFind(hash, buck, needle, nlen);
		if (hit)
			return hit;
	}

	for (int i = 0; i < size; ++i)
	{
		const std::uint64_t hit = StrtChainFind(hash, i, needle, nlen);
		if (hit)
			return hit;
	}

	return 0;
}

bool TableValStrEq(std::uint64_t val_node, const char* s, std::size_t len)
{
	const int tt = g_Memory.Read<std::int32_t>(val_node + 12) & 0xF;
	if (tt != 6)
		return false;

	const std::uint64_t ts = g_Memory.Read<std::uint64_t>(val_node);
	if (g_fval_ts)
		return ts == g_fval_ts;

	return TsEq(ts, s, len);
}

bool LooksLikeTable(std::uint64_t o);

// любая string-value == needle (для ammo без точного _tag)
bool TableHasStrVal(std::uint64_t tbl, const char* needle, std::size_t nlen)
{
	if (!LooksLikeTable(tbl))
		return false;

	const std::uint8_t lsz = g_Memory.Read<std::uint8_t>(tbl + 3);
	const std::uint64_t node = g_Memory.Read<std::uint64_t>(tbl + 24);
	const int n = 1 << lsz;

	for (int i = 0; i < n; ++i)
	{
		const std::uint64_t nd = node + (std::uint64_t)i * 32ull;
		if (!TableValStrEq(nd, needle, nlen))
			continue;
		return true;
	}

	return false;
}

// node val @0, key ptr @16, key tt:4|next:28 @28; TValue tt @12
bool TableFindStr(std::uint64_t tbl, const char* key, std::uint64_t& val_node)
{
	if (!g_Memory.IsValid(tbl))
		return false;

	std::uint8_t lsz = 0;
	std::uint64_t node = 0;

	if (g_hdr_tbl == tbl)
	{
		lsz = g_hdr_lsz;
		node = g_hdr_node;
	}

	else
	{
		lsz = g_Memory.Read<std::uint8_t>(tbl + 3);
		if (lsz == 0 || lsz > 18)
			return false;

		node = g_Memory.Read<std::uint64_t>(tbl + 24);
		if (!g_Memory.IsValid(node))
			return false;

		const std::uint64_t empty = (std::uint64_t)g_Memory.GetModuleBase() + 0x6bc02c8ull;
		if (node == empty)
			return false;

		g_hdr_tbl = tbl;
		g_hdr_lsz = lsz;
		g_hdr_node = node;
	}

	const int n = 1 << lsz;
	const std::size_t klen = key ? std::strlen(key) : 0;

	// luau mainposition + next chain — не жрать весь node[]
	if (g_fkey_ts && n > 0)
	{
		unsigned int h = g_fkey_hash;
		if (!h)
			h = g_Memory.Read<unsigned int>(g_fkey_ts + 16);

		int i = (int)(h & (unsigned int)(n - 1));
		unsigned char nb[32];
		for (int hop = 0; hop < n && hop < 256; ++hop)
		{
			const std::uint64_t nd = node + (std::uint64_t)i * 32ull;
			if (g_Memory.ReadRaw((uintptr_t)nd, nb, 32) != 32)
				break;

			const std::uint32_t kw = (std::uint32_t)nb[28]
				| ((std::uint32_t)nb[29] << 8)
				| ((std::uint32_t)nb[30] << 16)
				| ((std::uint32_t)nb[31] << 24);
			const std::uint8_t ktt = (std::uint8_t)(kw & 0xF);
			if (ktt == 6)
			{
				std::uint64_t ts = 0;
				std::memcpy(&ts, nb + 16, 8);
				if (ts == g_fkey_ts)
				{
					val_node = nd;
					return true;
				}
			}

			const std::int32_t nx = (std::int32_t)kw >> 4;
			if (nx == 0)
				break;

			i += nx;
			if (i < 0 || i >= n)
				break;
		}
		// hash miss — ниже линейный (оффсет hash кривой / dead key)
	}

	// один ReadRaw на чанк — RPM/slot убивал findgc
	unsigned char buf[32 * 64];
	int off = 0;
	while (off < n)
	{
		int chunk = n - off;
		if (chunk > 64)
			chunk = 64;

		const std::size_t bytes = (std::size_t)chunk * 32ull;
		if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
			return false;

		for (int i = 0; i < chunk; ++i)
		{
			const unsigned char* nd = buf + (std::size_t)i * 32ull;
			const std::uint8_t ktt = (std::uint8_t)(nd[28] & 0xF);
			if (ktt != 6)
				continue;

			std::uint64_t ts = 0;
			std::memcpy(&ts, nd + 16, 8);

			if (g_fkey_ts)
			{
				if (ts != g_fkey_ts)
					continue;
			}

			else if (!key || !TsEq(ts, key, klen))
			{
				continue;
			}

			val_node = node + (std::uint64_t)(off + i) * 32ull;
			return true;
		}

		off += chunk;
	}

	return false;
}

bool LooksLikeTable(std::uint64_t o)
{
	if (!g_Memory.IsValid(o) || (o & 0x7))
		return false;

	if (g_Memory.Read<std::uint8_t>(o + 1) != 7)
		return false;

	// без hash — мусор с page walk, ammo всё равно с ключами
	const std::uint8_t lsz = g_Memory.Read<std::uint8_t>(o + 3);
	if (lsz == 0 || lsz > 18)
		return false;

	const std::int32_t sz = g_Memory.Read<std::int32_t>(o + 8);
	if (sz < 0 || sz > 1000000)
		return false;

	const std::uint64_t arr = g_Memory.Read<std::uint64_t>(o + 32);
	const std::uint64_t node = g_Memory.Read<std::uint64_t>(o + 24);
	if (arr && !g_Memory.IsValid(arr))
		return false;
	if (!g_Memory.IsValid(node))
		return false;

	const std::uint64_t empty = (std::uint64_t)g_Memory.GetModuleBase() + 0x6bc02c8ull;
	if (node == empty)
		return false;

	g_hdr_tbl = o;
	g_hdr_lsz = lsz;
	g_hdr_node = node;
	return true;
}

std::uint64_t ProxyAddr(lua_State* L, int idx)
{
	lua_getfield(L, LUA_REGISTRYINDEX, "jp_gtables");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return 0;
	}

	lua_pushvalue(L, idx);
	lua_rawget(L, -2);
	const std::uint64_t a = (std::uint64_t)lua_tointeger(L, -1);
	lua_pop(L, 2);
	return a;
}

void PushGameTValue(lua_State* L, std::uint64_t tv);

void PushTableProxy(lua_State* L, std::uint64_t addr)
{
	lua_newtable(L);

	luaL_getmetatable(L, "jp.luau_table");
	if (lua_isnil(L, -1))
	{
		lua_pop(L, 1);
		return;
	}
	lua_setmetatable(L, -2);

	lua_getfield(L, LUA_REGISTRYINDEX, "jp_gtables");
	if (!lua_istable(L, -1))
	{
		lua_pop(L, 1);
		return;
	}
	lua_pushvalue(L, -2);
	lua_pushinteger(L, (lua_Integer)addr);
	lua_rawset(L, -3);
	lua_pop(L, 1);
}

void PushGameTValue(lua_State* L, std::uint64_t tv)
{
	const int tt = g_Memory.Read<std::int32_t>(tv + 12);

	if (tt == 0)
	{
		lua_pushnil(L);
		return;
	}

	if (tt == 1)
	{
		lua_pushboolean(L, g_Memory.Read<std::int32_t>(tv) != 0);
		return;
	}

	if (tt == 3)
	{
		lua_pushnumber(L, g_Memory.Read<double>(tv));
		return;
	}

	if (tt == 6)
	{
		const std::uint64_t ts = g_Memory.Read<std::uint64_t>(tv);
		if (!g_Memory.IsValid(ts))
		{
			lua_pushnil(L);
			return;
		}

		const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
		if (len > 0x10000)
		{
			lua_pushnil(L);
			return;
		}

		if (len == 0)
		{
			lua_pushlstring(L, "", 0);
			return;
		}

		char stack[256];
		char* buf = stack;
		std::string heap;
		if (len > sizeof(stack))
		{
			heap.resize(len);
			buf = heap.data();
		}

		if (g_Memory.ReadRaw((uintptr_t)(ts + 24), buf, len) != len)
		{
			lua_pushnil(L);
			return;
		}

		lua_pushlstring(L, buf, len);
		return;
	}

	if (tt == 7)
	{
		const std::uint64_t t = g_Memory.Read<std::uint64_t>(tv);
		if (LooksLikeTable(t))
			PushTableProxy(L, t);

		else
			lua_pushnil(L);

		return;
	}

	lua_pushnil(L);
}

int proxy_index(lua_State* L)
{
	const std::uint64_t tbl = ProxyAddr(L, 1);
	if (!tbl)
	{
		lua_pushnil(L);
		return 1;
	}

	const char* key = lua_tostring(L, 2);
	if (!key)
	{
		lua_pushnil(L);
		return 1;
	}

	std::uint64_t nd = 0;
	if (!TableFindStr(tbl, key, nd))
	{
		lua_pushnil(L);
		return 1;
	}

	PushGameTValue(L, nd);
	return 1;
}

int proxy_newindex(lua_State* L)
{
	const std::uint64_t tbl = ProxyAddr(L, 1);
	if (!tbl)
		return 0;

	const char* key = lua_tostring(L, 2);
	if (!key)
		return 0;

	const std::size_t klen = std::strlen(key);
	const std::uint64_t id = PcId(tbl, key, klen);
	std::uint64_t nd = 0;

	auto it = g_pc.find(id);
	if (it != g_pc.end())
		nd = it->second;

	else
	{
		if (!TableFindStr(tbl, key, nd))
			return 0;

		if (g_pc.size() > 4096)
			g_pc.clear();
		g_pc[id] = nd;
	}

	// пока number / bool, хватит для ammo
	if (lua_type(L, 3) == LUA_TNUMBER)
	{
		const double n = lua_tonumber(L, 3);
		g_Memory.Write<double>(nd + 0, n);
		g_Memory.Write<std::int32_t>(nd + 12, 3);
		return 0;
	}

	if (lua_type(L, 3) == LUA_TBOOLEAN)
	{
		g_Memory.Write<std::int32_t>(nd + 0, lua_toboolean(L, 3) ? 1 : 0);
		g_Memory.Write<std::int32_t>(nd + 12, 1);
		return 0;
	}

	return 0;
}

int l_rawget_proxy(lua_State* L)
{
	luaL_checkany(L, 1);
	luaL_checkany(L, 2);

	if (lua_istable(L, 1) && ProxyAddr(L, 1) != 0 && lua_type(L, 2) == LUA_TSTRING)
	{
		const std::uint64_t tbl = ProxyAddr(L, 1);
		const char* key = lua_tostring(L, 2);
		std::uint64_t nd = 0;
		if (key && TableFindStr(tbl, key, nd))
		{
			PushGameTValue(L, nd);
			return 1;
		}

		lua_pushnil(L);
		return 1;
	}

	lua_pushvalue(L, lua_upvalueindex(1));
	lua_pushvalue(L, 1);
	lua_pushvalue(L, 2);
	lua_call(L, 2, 1);
	return 1;
}

int getgc_iter(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);
	const lua_Integer i = luaL_checkinteger(L, 2) + 1;
	lua_rawgeti(L, 1, i);
	if (lua_isnil(L, -1))
		return 0;

	lua_pushinteger(L, i);
	lua_insert(L, -2);
	return 2;
}

bool TryAddTable(
	lua_State* L,
	std::uint64_t obj,
	int& n,
	int maxn,
	std::unordered_set<std::uint64_t>& seen)
{
	if (n >= maxn)
		return false;
	if (!LooksLikeTable(obj))
		return false;

	if (g_fval_only && g_fval)
	{
		if (!TableHasStrVal(obj, g_fval, std::strlen(g_fval)))
			return false;
	}

	else if (g_fkey)
	{
		std::uint64_t nd = 0;
		if (!TableFindStr(obj, g_fkey, nd))
			return false;

		if (g_fval)
		{
			if (!TableValStrEq(nd, g_fval, std::strlen(g_fval)))
				return false;
		}
	}

	if (!seen.insert(obj).second)
		return false;

	if (g_fkey || g_fval_only)
		g_cached_find = obj;

	PushTableProxy(L, obj);
	lua_rawseti(L, -2, ++n);
	return true;
}

void CollectTablesFromGcoList(
	lua_State* L,
	std::uint64_t head,
	int& n,
	int maxn,
	std::unordered_set<std::uint64_t>& seen)
{
	std::uint64_t cur = head;
	for (int i = 0; i < 80000 && n < maxn; ++i)
	{
		if (!g_Memory.IsValid(cur) || (cur & 0x7))
			break;

		const int tt = (int)g_Memory.Read<std::uint8_t>(cur + 1);
		if (tt < 0 || tt > 15)
			break;

		if (tt == 7)
			TryAddTable(L, cur, n, maxn, seen);

		const std::uint64_t nxt = GcoNext(cur, tt);
		if (nxt == cur)
			break;
		cur = nxt;
	}
}

void CollectTablesFromPageChain(
	lua_State* L,
	std::uint64_t head,
	std::uint64_t sentinel,
	int& n,
	int maxn,
	std::unordered_set<std::uint64_t>& seen,
	int lim,
	int next_off)
{
	std::uint64_t p = head;
	for (int pi = 0; pi < lim && n < maxn; ++pi)
	{
		if (!p || p == sentinel)
			break;
		if (!g_Memory.IsValid(p))
			break;

		const std::int32_t pageSize = g_Memory.Read<std::int32_t>(p + 32);
		const std::int32_t blockSize = g_Memory.Read<std::int32_t>(p + 36);

		if (blockSize >= 40 && blockSize <= 512 && pageSize >= 128 && pageSize <= 65536)
		{
			const int cap = (pageSize - 64) / blockSize;
			if (cap > 0 && cap < 20000)
			{
				for (int i = 0; i < cap && n < maxn; ++i)
				{
					const std::uint64_t obj = p + 64ull + (std::uint64_t)i * (std::uint64_t)blockSize;
					TryAddTable(L, obj, n, maxn, seen);
				}
			}
		}

		const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(p + (std::uint64_t)next_off);
		if (nxt == p)
			break;
		p = nxt;
	}
}

int CollectGameTables(
	lua_State* L,
	std::uint64_t G,
	int maxn,
	int& n,
	std::unordered_set<std::uint64_t>& seen)
{
	if (g_cached_find && n < maxn)
		TryAddTable(L, g_cached_find, n, maxn, seen);

	// gray тёплый, потом один полный page list — freelist+sizeclass дубли и жрут RPM
	CollectTablesFromGcoList(L, g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gray), n, maxn, seen);
	CollectTablesFromGcoList(L, g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::grayagain), n, maxn, seen);
	CollectTablesFromGcoList(L, g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::weak), n, maxn, seen);

	if (n >= maxn)
		return n;

	CollectTablesFromPageChain(
		L,
		g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages_large),
		0, n, maxn, seen, 200000, (int)Offsets::LuauGlobal::page_next_all);

	return n;
}

// getgc(true) -> for _,v in getgc(true) do  (iter, arr, 0)
// getgc() / false -> getgc_info stats
int l_getgc(lua_State* L)
{
	const int top = lua_gettop(L);
	const bool want_tables = (top >= 1) && lua_toboolean(L, 1);

	if (!want_tables)
		return l_getgc_info(L);

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	g_fkey = nullptr;
	g_fval = nullptr;
	lua_createtable(L, 256, 0);
	int n = 0;
	std::unordered_set<std::uint64_t> seen;
	seen.reserve(8192);
	for (const auto& vm : vms)
		CollectGameTables(L, vm.G, 120000, n, seen);

	lua_pushcfunction(L, getgc_iter);
	lua_insert(L, -2);
	lua_pushinteger(L, 0);
	return 3;
}

// findgc({"FireRate","RPM",...}) — один walk, map key -> {proxies}
// иначе findgc(key) / findgc(key,val) / findgc(nil,val)
int l_findgc_keys(lua_State* L)
{
	luaL_checktype(L, 1, LUA_TTABLE);

	struct KEnt
	{
		const char* s;
		std::size_t len;
		std::uint64_t ts;
		unsigned int hash;
		int n; // hits
	};

	std::vector<KEnt> keys;
	keys.reserve(64);
	const int narg = (int)lua_rawlen(L, 1);
	for (int i = 1; i <= narg && (int)keys.size() < 64; ++i)
	{
		lua_rawgeti(L, 1, i);
		if (lua_type(L, -1) == LUA_TSTRING)
		{
			KEnt e{};
			e.s = lua_tostring(L, -1);
			e.len = e.s ? std::strlen(e.s) : 0;
			e.ts = 0;
			e.hash = 0;
			e.n = 0;
			if (e.s && e.len)
				keys.push_back(e);
		}
		lua_pop(L, 1);
	}

	if (keys.empty())
	{
		lua_createtable(L, 0, 0);
		return 1;
	}

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
	{
		return a.tb < b.tb;
	});

	// out[key] = array
	lua_createtable(L, 0, (int)keys.size());
	const int out = lua_gettop(L);
	for (std::size_t ki = 0; ki < keys.size(); ++ki)
	{
		lua_createtable(L, 8, 0);
		lua_setfield(L, out, keys[ki].s);
	}

	const int per_key = 400;
	std::unordered_set<std::uint64_t> seen;
	seen.reserve(8192);
	const std::uint64_t empty_node =
		(std::uint64_t)g_Memory.GetModuleBase() + 0x6bc02c8ull;

	// одна таблица = один скан node[], все ключи сразу
	auto try_one = [&](std::uint64_t obj)
	{
		if (!LooksLikeTable(obj))
			return;
		if (!seen.insert(obj).second)
			return;

		int left = 0;
		for (std::size_t ki = 0; ki < keys.size(); ++ki)
		{
			if (keys[ki].ts && keys[ki].n < per_key)
				++left;
		}

		if (!left)
			return;

		const std::uint8_t lsz = g_Memory.Read<std::uint8_t>(obj + 3);
		if (lsz == 0 || lsz > 18)
			return;

		const std::uint64_t node = g_Memory.Read<std::uint64_t>(obj + 24);
		if (!g_Memory.IsValid(node) || node == empty_node)
			return;

		const int nn = 1 << lsz;
		unsigned char got[64];
		std::memset(got, 0, sizeof(got));
		int hit_here = 0;

		unsigned char buf[32 * 64];
		int off = 0;
		while (off < nn && hit_here < left)
		{
			int chunk = nn - off;
			if (chunk > 64)
				chunk = 64;

			const std::size_t bytes = (std::size_t)chunk * 32ull;
			if (g_Memory.ReadRaw((uintptr_t)(node + (std::uint64_t)off * 32ull), buf, bytes) != bytes)
				return;

			for (int i = 0; i < chunk; ++i)
			{
				const unsigned char* nd = buf + (std::size_t)i * 32ull;
				if ((std::uint8_t)(nd[28] & 0xF) != 6)
					continue;

				std::uint64_t ts = 0;
				std::memcpy(&ts, nd + 16, 8);

				for (std::size_t ki = 0; ki < keys.size(); ++ki)
				{
					KEnt& e = keys[ki];
					if (!e.ts || e.n >= per_key || got[ki])
						continue;

					if (ts != e.ts)
						continue;

					got[ki] = 1;
					++hit_here;
					lua_getfield(L, out, e.s);
					PushTableProxy(L, obj);
					lua_rawseti(L, -2, ++e.n);
					lua_pop(L, 1);
					g_cached_find = obj;
				}
			}

			off += chunk;
		}
	};

	auto walk_gco = [&](std::uint64_t head)
	{
		std::uint64_t cur = head;
		for (int i = 0; i < 80000; ++i)
		{
			if (!g_Memory.IsValid(cur) || (cur & 0x7))
				break;

			const int tt = (int)g_Memory.Read<std::uint8_t>(cur + 1);
			if (tt < 0 || tt > 15)
				break;

			if (tt == 7)
				try_one(cur);

			const std::uint64_t nxt = GcoNext(cur, tt);
			if (nxt == cur)
				break;
			cur = nxt;
		}
	};

	auto walk_pages = [&](std::uint64_t head)
	{
		std::uint64_t p = head;
		for (int pi = 0; pi < 200000; ++pi)
		{
			if (!p || !g_Memory.IsValid(p))
				break;

			const std::int32_t pageSize = g_Memory.Read<std::int32_t>(p + 32);
			const std::int32_t blockSize = g_Memory.Read<std::int32_t>(p + 36);
			if (blockSize >= 40 && blockSize <= 512 && pageSize >= 128 && pageSize <= 65536)
			{
				const int cap = (pageSize - 64) / blockSize;
				if (cap > 0 && cap < 20000)
				{
					for (int i = 0; i < cap; ++i)
					{
						const std::uint64_t obj =
							p + 64ull + (std::uint64_t)i * (std::uint64_t)blockSize;
						try_one(obj);
					}
				}
			}

			const std::uint64_t nxt =
				g_Memory.Read<std::uint64_t>(p + Offsets::LuauGlobal::page_next_all);
			if (nxt == p)
				break;
			p = nxt;
		}
	};

	for (const auto& vm : vms)
	{
		int alive = 0;
		for (std::size_t ki = 0; ki < keys.size(); ++ki)
		{
			keys[ki].ts = FindStrtFirst(vm.G, keys[ki].s);
			if (keys[ki].ts)
			{
				keys[ki].hash = g_Memory.Read<unsigned int>(keys[ki].ts + 16);
				++alive;
			}
		}

		if (!alive)
			continue;

		seen.clear();
		if (g_cached_find)
			try_one(g_cached_find);

		walk_gco(g_Memory.Read<std::uint64_t>(vm.G + Offsets::LuauGlobal::gray));
		walk_gco(g_Memory.Read<std::uint64_t>(vm.G + Offsets::LuauGlobal::grayagain));
		walk_gco(g_Memory.Read<std::uint64_t>(vm.G + Offsets::LuauGlobal::weak));
		walk_pages(g_Memory.Read<std::uint64_t>(vm.G + Offsets::LuauGlobal::gcopages_large));
	}

	g_fkey = nullptr;
	g_fkey_ts = 0;
	g_fkey_hash = 0;
	g_fval = nullptr;
	g_fval_only = false;
	return 1;
}

int l_findgc(lua_State* L)
{
	if (lua_istable(L, 1))
		return l_findgc_keys(L);

	const char* key = nullptr;
	const char* val = nullptr;
	bool val_only = false;

	if (lua_type(L, 1) == LUA_TSTRING)
		key = lua_tostring(L, 1);

	else if (lua_isnoneornil(L, 1) && lua_type(L, 2) == LUA_TSTRING)
	{
		val = lua_tostring(L, 2);
		val_only = true;
	}

	else
		return luaL_error(L, "findgc(key|keys) or findgc(key,val) or findgc(nil,val)");

	if (!val_only && lua_gettop(L) >= 2 && lua_type(L, 2) == LUA_TSTRING)
		val = lua_tostring(L, 2);

	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	// value/точное имя — большой VM первым (локальная пушка там)
	// иначе мелкий первым — ammo ~1MB, roact ~90MB
	if (val || val_only)
	{
		std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
		{
			return a.tb > b.tb;
		});
	}

	else
	{
		std::sort(vms.begin(), vms.end(), [](const GameVm& a, const GameVm& b)
		{
			return a.tb < b.tb;
		});
	}

	g_fkey = key;
	g_fval = val;
	g_fval_only = val_only;
	g_fkey_hash = 0;
	lua_createtable(L, 8, 0);
	int n = 0;
	std::unordered_set<std::uint64_t> seen;
	// кап общий + бюджет на VM — мелкий не сожрёт всё до большого
	const int cap = (val || val_only) ? 8000 : 5000;
	const int per_vm = (val || val_only) ? 4000 : 2500;

	for (const auto& vm : vms)
	{
		g_fkey_ts = 0;
		g_fval_ts = 0;
		g_fkey_hash = 0;

		if (key)
		{
			g_fkey_ts = FindStrtFirst(vm.G, key);
			if (!g_fkey_ts)
				continue;

			g_fkey_hash = g_Memory.Read<unsigned int>(g_fkey_ts + 16);
		}

		if (val)
		{
			g_fval_ts = FindStrtFirst(vm.G, val);
			if (!g_fval_ts)
				continue;
		}

		int local_cap = n + per_vm;
		if (local_cap > cap)
			local_cap = cap;
		CollectGameTables(L, vm.G, local_cap, n, seen);
		if (n >= cap)
			break;
	}

	g_fkey = nullptr;
	g_fval = nullptr;
	g_fval_only = false;
	g_fkey_ts = 0;
	g_fval_ts = 0;
	g_fkey_hash = 0;

	lua_pushcfunction(L, getgc_iter);
	lua_insert(L, -2);
	lua_pushinteger(L, 0);
	return 3;
}

int CountStrtNeedle(std::uint64_t G, const char* needle)
{
	const int size = g_Memory.Read<std::int32_t>(G + Offsets::LuauGlobal::strt_size);
	const std::uint64_t hash = g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::strt_hash);
	if (size <= 0 || size > (1 << 22) || !g_Memory.IsValid(hash))
		return -1;

	const std::size_t nlen = std::strlen(needle);
	int hits = 0;

	for (int i = 0; i < size; ++i)
	{
		std::uint64_t ts = g_Memory.Read<std::uint64_t>(hash + (std::uint64_t)i * 8ull);
		for (int k = 0; k < 100000 && ts; ++k)
		{
			if (!g_Memory.IsValid(ts) || (ts & 7))
				break;

			if (TsEq(ts, needle, nlen))
				++hits;

			const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(ts + 8);
			if (nxt == ts)
				break;
			ts = nxt;
		}
	}

	return hits;
}

int CountPageStrings(std::uint64_t G, const char* needle, int lim)
{
	const std::size_t nlen = std::strlen(needle);
	int hits = 0;
	int seen = 0;

	auto scan_chain = [&](std::uint64_t head, std::uint64_t sentinel, int chain_lim, int next_off)
	{
		std::uint64_t p = head;
		for (int pi = 0; pi < chain_lim && hits < lim; ++pi)
		{
			if (!p || p == sentinel)
				break;
			if (!g_Memory.IsValid(p))
				break;

			const std::int32_t pageSize = g_Memory.Read<std::int32_t>(p + 32);
			const std::int32_t blockSize = g_Memory.Read<std::int32_t>(p + 36);
			if (blockSize >= 24 && blockSize <= 512 && pageSize >= 128 && pageSize <= 65536)
			{
				const int cap = (pageSize - 64) / blockSize;
				if (cap > 0 && cap < 20000)
				{
					for (int i = 0; i < cap && hits < lim; ++i)
					{
						const std::uint64_t obj = p + 64ull + (std::uint64_t)i * (std::uint64_t)blockSize;
						if (g_Memory.Read<std::uint8_t>(obj + 1) != 6)
							continue;
						++seen;
						if (TsEq(obj, needle, nlen))
							++hits;
					}
				}
			}

			const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(p + (std::uint64_t)next_off);
			if (nxt == p)
				break;
			p = nxt;
		}
	};

	scan_chain(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages),
		G + Offsets::LuauGlobal::gcopages_end, 200000, (int)Offsets::LuauGlobal::page_next_free);
	scan_chain(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages_large),
		0, 200000, (int)Offsets::LuauGlobal::page_next_all);
	for (int sc = 0; sc < 48; ++sc)
	{
		scan_chain(g_Memory.Read<std::uint64_t>(
			G + Offsets::LuauGlobal::gcopages_sizeclass + (std::uint64_t)sc * 8ull),
			0, 20000, (int)Offsets::LuauGlobal::page_next_free);
	}

	(void)seen;
	return hits;
}

void VisitAllTables(std::uint64_t G, const std::function<void(std::uint64_t)>& fn)
{
	auto walk_gco = [&](std::uint64_t head)
	{
		std::uint64_t cur = head;
		for (int i = 0; i < 80000; ++i)
		{
			if (!g_Memory.IsValid(cur) || (cur & 0x7))
				break;
			const int tt = (int)g_Memory.Read<std::uint8_t>(cur + 1);
			if (tt < 0 || tt > 15)
				break;
			if (tt == 7)
				fn(cur);
			const std::uint64_t nxt = GcoNext(cur, tt);
			if (nxt == cur)
				break;
			cur = nxt;
		}
	};

	walk_gco(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gray));
	walk_gco(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::grayagain));
	walk_gco(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::weak));

	auto walk_pages = [&](std::uint64_t head, std::uint64_t sentinel, int lim, int next_off)
	{
		std::uint64_t p = head;
		for (int pi = 0; pi < lim; ++pi)
		{
			if (!p || p == sentinel)
				break;
			if (!g_Memory.IsValid(p))
				break;

			const std::int32_t pageSize = g_Memory.Read<std::int32_t>(p + 32);
			const std::int32_t blockSize = g_Memory.Read<std::int32_t>(p + 36);
			if (blockSize >= 40 && blockSize <= 512 && pageSize >= 128 && pageSize <= 65536)
			{
				const int cap = (pageSize - 64) / blockSize;
				if (cap > 0 && cap < 20000)
				{
					for (int i = 0; i < cap; ++i)
					{
						const std::uint64_t obj = p + 64ull + (std::uint64_t)i * (std::uint64_t)blockSize;
						fn(obj);
					}
				}
			}

			const std::uint64_t nxt = g_Memory.Read<std::uint64_t>(p + (std::uint64_t)next_off);
			if (nxt == p)
				break;
			p = nxt;
		}
	};

	walk_pages(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages),
		G + Offsets::LuauGlobal::gcopages_end, 200000, (int)Offsets::LuauGlobal::page_next_free);
	walk_pages(g_Memory.Read<std::uint64_t>(G + Offsets::LuauGlobal::gcopages_large),
		0, 200000, (int)Offsets::LuauGlobal::page_next_all);
	for (int sc = 0; sc < 48; ++sc)
	{
		walk_pages(g_Memory.Read<std::uint64_t>(
			G + Offsets::LuauGlobal::gcopages_sizeclass + (std::uint64_t)sc * 8ull),
			0, 20000, (int)Offsets::LuauGlobal::page_next_free);
	}
}

// gcprobe() — все VM + где ammo строка
int l_gcprobe(lua_State* L)
{
	std::vector<GameVm> vms;
	CollectGameVms(vms);
	if (vms.empty())
	{
		lua_pushnil(L);
		lua_pushstring(L, "no game lua state");
		return 2;
	}

	lua_createtable(L, 0, 8);
	lua_pushinteger(L, (lua_Integer)vms.size());
	lua_setfield(L, -2, "vm_count");

	lua_createtable(L, (int)vms.size(), 0);
	int ammo_vm = -1;

	for (int vi = 0; vi < (int)vms.size(); ++vi)
	{
		const auto& vm = vms[vi];
		const std::uint64_t G = vm.G;

		int n_hash = 0;
		int n_strkeys = 0;
		int n_tag_key = 0;
		int n_ammo_val = 0;
		std::unordered_set<std::uint64_t> seen;

		lua_createtable(L, 0, 16);
		lua_createtable(L, 20, 0);
		const int samples = lua_gettop(L);

		VisitAllTables(G, [&](std::uint64_t obj)
		{
			if (!LooksLikeTable(obj))
				return;
			if (!seen.insert(obj).second)
				return;

			++n_hash;

			const std::uint8_t lsz = g_Memory.Read<std::uint8_t>(obj + 3);
			const std::uint64_t node = g_Memory.Read<std::uint64_t>(obj + 24);
			const int nn = 1 << lsz;

			for (int i = 0; i < nn; ++i)
			{
				const std::uint64_t nd = node + (std::uint64_t)i * 32ull;
				const std::uint8_t ktt = (std::uint8_t)(g_Memory.Read<std::uint8_t>(nd + 28) & 0xF);
				if (ktt != 6)
					continue;

				const std::uint64_t ts = g_Memory.Read<std::uint64_t>(nd + 16);
				char sbuf[96]{};
				const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
				if (!g_Memory.IsValid(ts) || len == 0 || len >= sizeof(sbuf))
					continue;
				if (g_Memory.Read<std::uint8_t>(ts + 1) != 6)
					continue;
				if (g_Memory.ReadRaw((uintptr_t)(ts + 24), sbuf, len) != len)
					continue;
				sbuf[len] = 0;

				++n_strkeys;
				if (len == 4 && std::memcmp(sbuf, "_tag", 4) == 0)
					++n_tag_key;

				const int vtt = g_Memory.Read<std::int32_t>(nd + 12);
				if (vtt == 6)
				{
					const std::uint64_t vs = g_Memory.Read<std::uint64_t>(nd);
					if (TsEq(vs, "jewsploit_ammo_test", 19))
						++n_ammo_val;
				}

				if (n_strkeys <= 15)
				{
					lua_pushlstring(L, sbuf, len);
					lua_rawseti(L, samples, n_strkeys);
				}
			}
		});

		lua_setfield(L, -2, "sample_keys");

		const int st_ammo = CountStrtNeedle(G, "jewsploit_ammo_test");
		const int pg_ammo = CountPageStrings(G, "jewsploit_ammo_test", 50);
		const int st_tag = CountStrtNeedle(G, "_tag");

		if (st_ammo > 0 || pg_ammo > 0 || n_tag_key > 0 || n_ammo_val > 0)
			ammo_vm = vi;

		lua_pushinteger(L, (lua_Integer)vm.wrap);
		lua_setfield(L, -2, "wrap");
		lua_pushinteger(L, (lua_Integer)(vm.tb >> 10));
		lua_setfield(L, -2, "kb");
		lua_pushinteger(L, n_hash);
		lua_setfield(L, -2, "hash_tables");
		lua_pushinteger(L, n_strkeys);
		lua_setfield(L, -2, "str_keys");
		lua_pushinteger(L, n_tag_key);
		lua_setfield(L, -2, "tag_keys");
		lua_pushinteger(L, n_ammo_val);
		lua_setfield(L, -2, "ammo_as_val");
		lua_pushinteger(L, st_tag);
		lua_setfield(L, -2, "strt_tag");
		lua_pushinteger(L, st_ammo);
		lua_setfield(L, -2, "strt_ammo");
		lua_pushinteger(L, pg_ammo);
		lua_setfield(L, -2, "page_ammo");

		char buf[32];
		std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)G);
		lua_pushstring(L, buf);
		lua_setfield(L, -2, "G");
		std::snprintf(buf, sizeof(buf), "0x%llX", (unsigned long long)vm.L);
		lua_pushstring(L, buf);
		lua_setfield(L, -2, "L");

		lua_rawseti(L, -2, vi + 1);
	}

	lua_setfield(L, -2, "vms");
	lua_pushinteger(L, ammo_vm + 1); // 1-based or 0 if none
	lua_setfield(L, -2, "ammo_vm");

	return 1;
}

// getrawkeys(proxy) -> { key = value, ... } только string keys
int l_getrawkeys(lua_State* L)
{
	const std::uint64_t tbl = ProxyAddr(L, 1);
	if (!tbl || !LooksLikeTable(tbl))
	{
		lua_createtable(L, 0, 0);
		return 1;
	}

	const std::uint8_t lsz = g_Memory.Read<std::uint8_t>(tbl + 3);
	const std::uint64_t node = g_Memory.Read<std::uint64_t>(tbl + 24);
	const int n = 1 << lsz;

	lua_createtable(L, 0, n > 0 ? n : 1);

	for (int i = 0; i < n; ++i)
	{
		const std::uint64_t nd = node + (std::uint64_t)i * 32ull;
		const std::uint8_t ktt = (std::uint8_t)(g_Memory.Read<std::uint8_t>(nd + 28) & 0xF);
		if (ktt != 6)
			continue;

		const std::uint64_t ts = g_Memory.Read<std::uint64_t>(nd + 16);
		if (!g_Memory.IsValid(ts) || g_Memory.Read<std::uint8_t>(ts + 1) != 6)
			continue;

		const std::uint32_t len = g_Memory.Read<std::uint32_t>(ts + 20);
		if (len == 0 || len > 128)
			continue;

		char buf[129]{};
		if (g_Memory.ReadRaw((uintptr_t)(ts + 24), buf, len) != len)
			continue;

		const int vtt = g_Memory.Read<std::int32_t>(nd + 12) & 0xF;
		if (vtt == 0)
			continue;

		lua_pushlstring(L, buf, len);
		PushGameTValue(L, nd);
		lua_settable(L, -3);
	}

	return 1;
}

void EnsureProxyMt(lua_State* L)
{
	luaL_newmetatable(L, "jp.luau_table");
	lua_pushcfunction(L, proxy_index);
	lua_setfield(L, -2, "__index");
	lua_pushcfunction(L, proxy_newindex);
	lua_setfield(L, -2, "__newindex");
	lua_pop(L, 1);

	lua_newtable(L);
	lua_setfield(L, LUA_REGISTRYINDEX, "jp_gtables");
}

} // namespace

void Register(lua_State* L)
{
	EnsureProxyMt(L);

	lua_pushcfunction(L, l_setgc);
	lua_setglobal(L, "setgc");
	lua_pushcfunction(L, l_getgc);
	lua_setglobal(L, "getgc");
	lua_pushcfunction(L, l_getgc_info);
	lua_setglobal(L, "getgc_info");
	lua_pushcfunction(L, l_findgc);
	lua_setglobal(L, "findgc");
	lua_pushcfunction(L, l_gcprobe);
	lua_setglobal(L, "gcprobe");
	lua_pushcfunction(L, l_getrawkeys);
	lua_setglobal(L, "getrawkeys");

	// rawget на прокси должен лезть в игру, не в пустую cheat-таблицу
	lua_getglobal(L, "rawget");
	if (lua_isfunction(L, -1))
	{
		lua_pushcclosure(L, l_rawget_proxy, 1);
		lua_setglobal(L, "rawget");
	}

	else
	{
		lua_pop(L, 1);
	}
}

} // namespace LuaGc
} // namespace Features
} // namespace Cheat
