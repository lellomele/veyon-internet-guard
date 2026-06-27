/*
 * InternetGuardPlugin.cpp
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Third-party Veyon plugin — not part of the official Veyon distribution.
 */

#include <QList>
#include <QProcess>
#include <QStringList>

#include "InternetGuardPlugin.h"

// Toolbar/menu icon shared by every feature of this plugin.
static const char* ICON_URL = ":/internet-guard/network-offline.png";

// Feature UUIDs. The toggle's UUID must stay stable (it is the plugin identity);
// the others only need to be unique and constant.
static const char* UID_TOGGLE    = "a4b3c2d1-e5f6-7890-abcd-ef1234567890";
static const char* UID_BLOCK_NET  = "b1b2c3d4-1111-2222-3333-444455556666";
static const char* UID_ALLOW_NET  = "c1c2c3d4-1111-2222-3333-444455556666";

// Firewall rule names — all prefixed "VeyonIG_" for reliable cleanup.
static const char* RULE_HTTP  = "VeyonIG_BlockHTTP";     // TCP  80
static const char* RULE_HTTPS = "VeyonIG_BlockHTTPS";    // TCP 443
static const char* RULE_DNS_U = "VeyonIG_BlockDNS_UDP";  // UDP  53
static const char* RULE_DNS_T = "VeyonIG_BlockDNS_TCP";  // TCP  53
static const char* RULE_QUIC  = "VeyonIG_BlockQUIC";     // UDP 443  (HTTP/3)
static const char* RULE_DOT_T = "VeyonIG_BlockDoT_TCP";  // TCP 853  (DNS-over-TLS)
static const char* RULE_DOT_U = "VeyonIG_BlockDoT_UDP";  // UDP 853  (DNS-over-TLS)
static const char* RULE_PROXY = "VeyonIG_BlockProxy";    // TCP 8080,8443,3128
static const char* RULE_LAN   = "VeyonIG_AllowLAN";      // allow localsubnet (all ports)

// -------------------------------------------------------------------------

InternetGuardPlugin::InternetGuardPlugin(QObject* parent) :
	QObject(parent),
	m_internetAccessFeature(
		QStringLiteral("InternetGuard"),
		Feature::Flag::Mode | Feature::Flag::AllComponents,
		Feature::Uid(QLatin1String(UID_TOGGLE)),
		Feature::Uid(),
		tr("Block Internet"),
		tr("Allow Internet"),
		tr("Block or allow internet access on the selected computers"),
		QLatin1String(ICON_URL)
	),
	m_blockInternetFeature(
		QStringLiteral("InternetGuardBlock"),
		Feature::Flag::Action | Feature::Flag::AllComponents,
		Feature::Uid(QLatin1String(UID_BLOCK_NET)),
		m_internetAccessFeature.uid(),
		tr("Block Internet"),
		{},
		tr("Block internet access on the selected computers"),
		QLatin1String(ICON_URL)
	),
	m_allowInternetFeature(
		QStringLiteral("InternetGuardAllow"),
		Feature::Flag::Action | Feature::Flag::AllComponents,
		Feature::Uid(QLatin1String(UID_ALLOW_NET)),
		m_internetAccessFeature.uid(),
		tr("Allow Internet"),
		{},
		tr("Restore internet access on the selected computers"),
		QLatin1String(ICON_URL)
	),
	m_features({ m_internetAccessFeature, m_blockInternetFeature, m_allowInternetFeature })
{
}

// Restore internet access on unload so stale firewall rules are never left behind.
InternetGuardPlugin::~InternetGuardPlugin()
{
	allowInternet();
}

bool InternetGuardPlugin::isOwnFeature(Feature::Uid featureUid) const
{
	return featureUid == m_internetAccessFeature.uid()
	    || featureUid == m_blockInternetFeature.uid()
	    || featureUid == m_allowInternetFeature.uid();
}

bool InternetGuardPlugin::controlFeature(Feature::Uid featureUid,
                                         Operation operation,
                                         const QVariantMap& arguments,
                                         const ComputerControlInterfaceList& computerControlInterfaces)
{
	Q_UNUSED(arguments)

	if (!isOwnFeature(featureUid))
		return false;

	// The toolbar toggle (Mode): Start = block, Stop = allow.
	if (featureUid == m_internetAccessFeature.uid())
	{
		if (operation == Operation::Start)
			sendFeatureMessage(FeatureMessage(featureUid, BlockInternetCommand), computerControlInterfaces);
		else if (operation == Operation::Stop)
			sendFeatureMessage(FeatureMessage(featureUid, AllowInternetCommand), computerControlInterfaces);
		else
			return false;
		return true;
	}

	// Per-selection actions are only triggered with Operation::Start.
	if (operation != Operation::Start)
		return false;

	if (featureUid == m_blockInternetFeature.uid())
		sendFeatureMessage(FeatureMessage(featureUid, BlockInternetCommand), computerControlInterfaces);
	else if (featureUid == m_allowInternetFeature.uid())
		sendFeatureMessage(FeatureMessage(featureUid, AllowInternetCommand), computerControlInterfaces);
	else
		return false;

	return true;
}

bool InternetGuardPlugin::handleFeatureMessage(VeyonServerInterface& server,
                                               const MessageContext& messageContext,
                                               const FeatureMessage& message)
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if (!isOwnFeature(message.featureUid()))
		return false;

	switch (VEYON_DECODE_COMMAND(message, Commands))
	{
	case BlockInternetCommand:  blockInternet();  return true;
	case AllowInternetCommand:  allowInternet();  return true;
	}

	return false;
}

// Turning the firewall on is essential: block rules have no effect while a
// profile is disabled, which is the typical cause of "one client was not
// blocked". Safe to run unconditionally.
void InternetGuardPlugin::ensureFirewallEnabled()
{
	runNetshBatch({
		{ QStringLiteral("advfirewall"), QStringLiteral("set"),
		  QStringLiteral("allprofiles"), QStringLiteral("state"), QStringLiteral("on") }
	});
}

// Launches all netsh commands in parallel, then collects results and logs
// any failures. Total wall time ≈ max(individual times) instead of sum.
void InternetGuardPlugin::runNetshBatch(const QList<QStringList>& batch)
{
	QList<QProcess*> procs;
	procs.reserve(batch.size());
	for (const auto& args : batch)
	{
		auto* p = new QProcess();
		p->start(QStringLiteral("netsh"), args);
		procs.append(p);
	}
	for (auto* p : procs)
	{
		if (!p->waitForStarted(5000))
			qWarning() << "[InternetGuard] netsh failed to start:" << p->errorString();
		else if (!p->waitForFinished(10000))
			qWarning() << "[InternetGuard] netsh timed out:" << p->arguments();
		else if (p->exitCode() != 0)
			qWarning() << "[InternetGuard] netsh failed:" << p->arguments()
			           << p->readAllStandardError();
		delete p;
	}
}

void InternetGuardPlugin::blockInternet()
{
	allowInternet();        // remove any leftover rules before re-adding
	ensureFirewallEnabled();

	runNetshBatch({
		// LAN allow rule — Windows Firewall prefers the more-specific remoteip
		// match, so local-subnet traffic is permitted even while internet is blocked.
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_LAN),
		  QStringLiteral("dir=out"), QStringLiteral("action=allow"),
		  QStringLiteral("protocol=any"), QStringLiteral("remoteip=localsubnet"),
		  QStringLiteral("profile=any") },

		// HTTP — TCP/80
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_HTTP),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=80"),
		  QStringLiteral("profile=any") },

		// HTTPS — TCP/443
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_HTTPS),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=443"),
		  QStringLiteral("profile=any") },

		// DNS — UDP/53
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_DNS_U),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=53"),
		  QStringLiteral("profile=any") },

		// DNS — TCP/53
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_DNS_T),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=53"),
		  QStringLiteral("profile=any") },

		// QUIC / HTTP3 — UDP/443
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_QUIC),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=443"),
		  QStringLiteral("profile=any") },

		// DNS over TLS — TCP/853
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_DOT_T),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=853"),
		  QStringLiteral("profile=any") },

		// DNS over TLS — UDP/853
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_DOT_U),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=UDP"), QStringLiteral("remoteport=853"),
		  QStringLiteral("profile=any") },

		// Proxy / alternate HTTP — TCP/8080,8443,3128
		{ QStringLiteral("advfirewall"), QStringLiteral("firewall"), QStringLiteral("add"), QStringLiteral("rule"),
		  QStringLiteral("name=") + QLatin1String(RULE_PROXY),
		  QStringLiteral("dir=out"), QStringLiteral("action=block"),
		  QStringLiteral("protocol=TCP"), QStringLiteral("remoteport=8080,8443,3128"),
		  QStringLiteral("profile=any") },
	});
}

void InternetGuardPlugin::allowInternet()
{
	const QStringList names = {
		QLatin1String(RULE_HTTP),  QLatin1String(RULE_HTTPS),
		QLatin1String(RULE_DNS_U), QLatin1String(RULE_DNS_T),
		QLatin1String(RULE_QUIC),  QLatin1String(RULE_DOT_T),
		QLatin1String(RULE_DOT_U), QLatin1String(RULE_PROXY),
		QLatin1String(RULE_LAN)
	};

	QList<QStringList> batch;
	batch.reserve(names.size());
	for (const QString& name : names)
	{
		batch.append({ QStringLiteral("advfirewall"), QStringLiteral("firewall"),
		               QStringLiteral("delete"), QStringLiteral("rule"),
		               QStringLiteral("name=") + name });
	}
	runNetshBatch(batch);
}
