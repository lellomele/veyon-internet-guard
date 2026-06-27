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

InternetGuardPlugin::InternetGuardPlugin(QObject* parent) :
	QObject(parent),
	m_internetAccessFeature(
		QStringLiteral("InternetGuard"),
		Feature::Flag::Mode | Feature::Flag::AllComponents,
		Feature::Uid(QStringLiteral("a4b3c2d1-e5f6-7890-abcd-ef1234567890")),
		Feature::Uid(),
		tr("Block Internet"),
		tr("Allow Internet"),
		tr("Block or allow internet access on student computers"),
		// PNG (not SVG): Veyon's Windows build ships no SVG icon engine,
		// so an SVG path would yield an empty QIcon — see resources.qrc.
		QStringLiteral(":/internet-guard/network-offline.png")
	),
	m_features({ m_internetAccessFeature })
{
}

// Restore internet access on unload so stale rules are never left behind.
InternetGuardPlugin::~InternetGuardPlugin()
{
	allowInternet();
}

bool InternetGuardPlugin::controlFeature(Feature::Uid featureUid,
                                                 Operation operation,
                                                 const QVariantMap& arguments,
                                                 const ComputerControlInterfaceList& computerControlInterfaces)
{
	Q_UNUSED(arguments)

	if (featureUid != m_internetAccessFeature.uid())
		return false;

	if (operation == Operation::Start)
	{
		sendFeatureMessage(FeatureMessage(featureUid, BlockInternetCommand), computerControlInterfaces);
		return true;
	}

	if (operation == Operation::Stop)
	{
		sendFeatureMessage(FeatureMessage(featureUid, AllowInternetCommand), computerControlInterfaces);
		return true;
	}

	return false;
}

bool InternetGuardPlugin::handleFeatureMessage(VeyonServerInterface& server,
                                                       const MessageContext& messageContext,
                                                       const FeatureMessage& message)
{
	Q_UNUSED(server)
	Q_UNUSED(messageContext)

	if (message.featureUid() != m_internetAccessFeature.uid())
		return false;

	if (VEYON_DECODE_COMMAND(message, Commands) == BlockInternetCommand)
	{
		blockInternet();
		return true;
	}

	if (VEYON_DECODE_COMMAND(message, Commands) == AllowInternetCommand)
	{
		allowInternet();
		return true;
	}

	return false;
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
		p->waitForFinished(10000);
		if (p->exitCode() != 0)
			qWarning() << "[InternetGuard] netsh failed:" << p->readAllStandardError();
		delete p;
	}
}

void InternetGuardPlugin::blockInternet()
{
	allowInternet(); // remove any leftover rules before re-adding

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
