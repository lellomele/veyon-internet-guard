/*
 * InternetGuardPlugin.cpp
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Third-party Veyon plugin — not part of the official Veyon distribution.
 */

#include "InternetGuardPlugin.h"

#ifdef Q_OS_WIN
#include "WindowsFirewall.h"
#endif

// Toolbar/menu icon shared by every feature of this plugin.
static const char* ICON_URL = ":/internet-guard/network-offline.png";

// Feature UUIDs. The toggle's UUID must stay stable (it is the plugin identity);
// the others only need to be unique and constant.
static const char* UID_TOGGLE    = "a4b3c2d1-e5f6-7890-abcd-ef1234567890";
static const char* UID_BLOCK_NET  = "b1b2c3d4-1111-2222-3333-444455556666";
static const char* UID_ALLOW_NET  = "c1c2c3d4-1111-2222-3333-444455556666";

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

// The actual firewall work lives in WindowsFirewall.cpp (COM API); these
// wrappers only add logging and keep the rest of the plugin platform-neutral.
// The rules deliberately block ports 80/443/53/853/8080/8443/3128 toward the
// LAN too: Windows Firewall gives Block rules precedence over Allow rules, so
// a local-subnet exemption cannot work — and would open internal proxies as
// an escape route anyway. Veyon's own ports are untouched, so teacher-student
// communication keeps working while internet is blocked.
void InternetGuardPlugin::blockInternet()
{
#ifdef Q_OS_WIN
	if (!WindowsFirewall::blockInternet())
		qWarning("[InternetGuard] some firewall block rules could not be applied");
#else
	qWarning("[InternetGuard] internet blocking is only implemented on Windows");
#endif
}

void InternetGuardPlugin::allowInternet()
{
#ifdef Q_OS_WIN
	if (!WindowsFirewall::allowInternet())
		qWarning("[InternetGuard] some firewall block rules could not be removed");
#else
	qWarning("[InternetGuard] internet blocking is only implemented on Windows");
#endif
}
