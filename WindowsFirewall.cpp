/*
 * WindowsFirewall.cpp
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Windows Firewall backend via the INetFwPolicy2 COM API.
 * See WindowsFirewall.h for the rationale.
 */

#include <QtGlobal>

#include "WindowsFirewall.h"

#include <windows.h>
#include <netfw.h>
#include <oleauto.h>

namespace
{

// ---------------------------------------------------------------------------
// COM plumbing
// ---------------------------------------------------------------------------

// GUIDs of the firewall COM objects, defined locally so no uuid import
// library is needed and both MinGW toolchains (g++ 7.3 and 13.x) link
// identically. These are stable, documented Windows identifiers.
const CLSID kClsidNetFwPolicy2 =
	{ 0xE2B3C97F, 0x6AE1, 0x41AC, { 0x81, 0x7A, 0xF6, 0xF9, 0x21, 0x66, 0xD7, 0xDD } };
const IID kIidINetFwPolicy2 =
	{ 0x98325047, 0xC671, 0x4174, { 0x8D, 0x81, 0xDE, 0xFC, 0xD3, 0xF0, 0x31, 0x86 } };
const CLSID kClsidNetFwRule =
	{ 0x2C5BC43E, 0x3369, 0x4C33, { 0xAB, 0x0C, 0xBE, 0x94, 0x69, 0x67, 0x7A, 0xF4 } };
const IID kIidINetFwRule =
	{ 0xAF230D27, 0xBABA, 0x4E42, { 0xAC, 0xED, 0xF5, 0x24, 0xF2, 0x2C, 0xFC, 0xE2 } };

// Minimal RAII wrapper for a COM interface pointer.
template<typename T>
class ComPtr
{
public:
	ComPtr() = default;
	~ComPtr() { if (m_ptr) m_ptr->Release(); }
	ComPtr(const ComPtr&) = delete;
	ComPtr& operator=(const ComPtr&) = delete;

	T* operator->() const { return m_ptr; }
	T* get() const { return m_ptr; }
	explicit operator bool() const { return m_ptr != nullptr; }
	// For out-parameters of creation functions; the pointer must be empty.
	void** out() { return reinterpret_cast<void**>(&m_ptr); }
	T** typedOut() { return &m_ptr; }

private:
	T* m_ptr = nullptr;
};

// RAII BSTR (the string type COM methods take/return).
class Bstr
{
public:
	explicit Bstr(const wchar_t* s) : m_bstr(SysAllocString(s)) {}
	~Bstr() { SysFreeString(m_bstr); }
	Bstr(const Bstr&) = delete;
	Bstr& operator=(const Bstr&) = delete;
	operator BSTR() const { return m_bstr; }

private:
	BSTR m_bstr;
};

// Per-call COM initialization. The plugin runs both on the Veyon Server
// worker thread (no COM yet -> we init/uninit) and on the Master GUI thread
// (Qt already initialized OLE/STA -> S_FALSE) - and if the thread happens to
// be MTA already, RPC_E_CHANGED_MODE just means "use the existing apartment"
// (the firewall objects are in-proc, apartment-agnostic).
class ComInit
{
public:
	ComInit()
	{
		const HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		m_mustUninit = SUCCEEDED(hr);
		m_usable = SUCCEEDED(hr) || hr == RPC_E_CHANGED_MODE;
		if (!m_usable)
			qWarning("[InternetGuard] CoInitializeEx failed (hr=0x%08lx)",
			         static_cast<unsigned long>(hr));
	}
	~ComInit() { if (m_mustUninit) CoUninitialize(); }
	ComInit(const ComInit&) = delete;
	ComInit& operator=(const ComInit&) = delete;

	bool usable() const { return m_usable; }

private:
	bool m_mustUninit = false;
	bool m_usable = false;
};

// ---------------------------------------------------------------------------
// Rule table
// ---------------------------------------------------------------------------

// Outbound block rules, all profiles, IPv4+IPv6 (no remote-address filter).
// Names are prefixed "VeyonIG_" for reliable identification and cleanup.
// The block is deliberately port-based and applies to the LAN as well:
// exempting the local subnet would leave internal proxies as an escape route,
// and Windows Firewall gives Block rules precedence over Allow rules anyway,
// so a "LAN allow" rule could never punch through these blocks.
struct RuleSpec
{
	const wchar_t* name;
	long protocol;             // 6 = TCP, 17 = UDP (NET_FW_IP_PROTOCOL_*)
	const wchar_t* remotePorts;
};

const RuleSpec kBlockRules[] = {
	{ L"VeyonIG_BlockHTTP",    NET_FW_IP_PROTOCOL_TCP, L"80" },
	{ L"VeyonIG_BlockHTTPS",   NET_FW_IP_PROTOCOL_TCP, L"443" },
	{ L"VeyonIG_BlockDNS_TCP", NET_FW_IP_PROTOCOL_TCP, L"53" },
	{ L"VeyonIG_BlockDNS_UDP", NET_FW_IP_PROTOCOL_UDP, L"53" },
	{ L"VeyonIG_BlockQUIC",    NET_FW_IP_PROTOCOL_UDP, L"443" },   // HTTP/3
	{ L"VeyonIG_BlockDoT_TCP", NET_FW_IP_PROTOCOL_TCP, L"853" },   // DNS-over-TLS
	{ L"VeyonIG_BlockDoT_UDP", NET_FW_IP_PROTOCOL_UDP, L"853" },   // DNS-over-TLS/QUIC
	{ L"VeyonIG_BlockProxy",   NET_FW_IP_PROTOCOL_TCP, L"8080,8443,3128" },
};

// Rules created by plugin <= 1.1 that are no longer added but must still be
// cleaned up when found (upgrades from an older version while blocked).
const wchar_t* kLegacyRuleNames[] = {
	L"VeyonIG_AllowLAN",
};

const wchar_t* kRuleDescription =
	L"Added by the Veyon InternetGuard plugin. Removed automatically when internet access is restored.";
const wchar_t* kRuleGroup = L"Veyon InternetGuard";

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Opens the firewall policy and its rule collection.
bool openFirewall(ComPtr<INetFwPolicy2>& policy, ComPtr<INetFwRules>& rules)
{
	HRESULT hr = CoCreateInstance(kClsidNetFwPolicy2, nullptr, CLSCTX_INPROC_SERVER,
	                              kIidINetFwPolicy2, policy.out());
	if (FAILED(hr))
	{
		qWarning("[InternetGuard] cannot access the Windows Firewall service (hr=0x%08lx)",
		         static_cast<unsigned long>(hr));
		return false;
	}

	hr = policy->get_Rules(rules.typedOut());
	if (FAILED(hr))
	{
		qWarning("[InternetGuard] cannot access the firewall rule collection (hr=0x%08lx)",
		         static_cast<unsigned long>(hr));
		return false;
	}

	return true;
}

// Turning the firewall on is essential: block rules have no effect while a
// profile is disabled, which is the typical cause of "one client was not
// blocked". Safe to run unconditionally.
void ensureFirewallEnabled(INetFwPolicy2* policy)
{
	static const NET_FW_PROFILE_TYPE2 profiles[] = {
		NET_FW_PROFILE2_DOMAIN, NET_FW_PROFILE2_PRIVATE, NET_FW_PROFILE2_PUBLIC
	};
	for (const auto profile : profiles)
	{
		const HRESULT hr = policy->put_FirewallEnabled(profile, VARIANT_TRUE);
		if (FAILED(hr))
			qWarning("[InternetGuard] cannot enable firewall profile %d (hr=0x%08lx)",
			         static_cast<int>(profile), static_cast<unsigned long>(hr));
	}
}

// Removes every rule with the given name. Rule names are not unique, so loop
// until no match remains (Item() is the reliable existence probe: Remove()
// reports success even when nothing matched). Missing rules are not an error.
bool removeRulesByName(INetFwRules* rules, const wchar_t* name)
{
	const Bstr bstrName(name);
	for (int guard = 0; guard < 64; ++guard)
	{
		ComPtr<INetFwRule> existing;
		if (FAILED(rules->Item(bstrName, existing.typedOut())))
			return true;    // no (more) rule with this name

		const HRESULT hr = rules->Remove(bstrName);
		if (FAILED(hr))
		{
			qWarning("[InternetGuard] cannot remove firewall rule '%ls' (hr=0x%08lx)",
			         name, static_cast<unsigned long>(hr));
			return false;
		}
	}
	return true;
}

bool addBlockRule(INetFwRules* rules, const RuleSpec& spec)
{
	ComPtr<INetFwRule> rule;
	HRESULT hr = CoCreateInstance(kClsidNetFwRule, nullptr, CLSCTX_INPROC_SERVER,
	                              kIidINetFwRule, rule.out());
	if (SUCCEEDED(hr)) hr = rule->put_Name(Bstr(spec.name));
	if (SUCCEEDED(hr)) hr = rule->put_Description(Bstr(kRuleDescription));
	if (SUCCEEDED(hr)) hr = rule->put_Grouping(Bstr(kRuleGroup));
	// The protocol must be set before the ports, or put_RemotePorts fails.
	if (SUCCEEDED(hr)) hr = rule->put_Protocol(spec.protocol);
	if (SUCCEEDED(hr)) hr = rule->put_RemotePorts(Bstr(spec.remotePorts));
	if (SUCCEEDED(hr)) hr = rule->put_Direction(NET_FW_RULE_DIR_OUT);
	if (SUCCEEDED(hr)) hr = rule->put_Action(NET_FW_ACTION_BLOCK);
	if (SUCCEEDED(hr)) hr = rule->put_Profiles(NET_FW_PROFILE2_ALL);
	if (SUCCEEDED(hr)) hr = rule->put_Enabled(VARIANT_TRUE);
	if (SUCCEEDED(hr)) hr = rules->Add(rule.get());

	if (FAILED(hr))
	{
		qWarning("[InternetGuard] cannot add firewall rule '%ls' (hr=0x%08lx)",
		         spec.name, static_cast<unsigned long>(hr));
		return false;
	}
	return true;
}

// Removes all InternetGuard rules (current and legacy) from an open collection.
bool removeAllRules(INetFwRules* rules)
{
	bool ok = true;
	for (const auto& spec : kBlockRules)
		ok = removeRulesByName(rules, spec.name) && ok;
	for (const auto* legacyName : kLegacyRuleNames)
		ok = removeRulesByName(rules, legacyName) && ok;
	return ok;
}

} // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

namespace WindowsFirewall
{

bool blockInternet()
{
	const ComInit com;
	if (!com.usable())
		return false;

	ComPtr<INetFwPolicy2> policy;
	ComPtr<INetFwRules> rules;
	if (!openFirewall(policy, rules))
		return false;

	removeAllRules(rules.get());        // idempotence: clear leftovers first
	ensureFirewallEnabled(policy.get());

	bool ok = true;
	for (const auto& spec : kBlockRules)
		ok = addBlockRule(rules.get(), spec) && ok;
	return ok;
}

bool allowInternet()
{
	const ComInit com;
	if (!com.usable())
		return false;

	ComPtr<INetFwPolicy2> policy;
	ComPtr<INetFwRules> rules;
	if (!openFirewall(policy, rules))
		return false;

	return removeAllRules(rules.get());
}

} // namespace WindowsFirewall
