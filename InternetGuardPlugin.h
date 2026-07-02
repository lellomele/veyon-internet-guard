/*
 * InternetGuardPlugin.h
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Third-party Veyon plugin — not part of the official Veyon distribution.
 */

#pragma once

#include "VeyonCompat.h"

class InternetGuardPlugin : public QObject, public FeatureProviderInterface, public PluginInterface
{
	Q_OBJECT
	Q_PLUGIN_METADATA(IID "io.veyon.Veyon.Plugins.InternetGuard")
	Q_INTERFACES(PluginInterface FeatureProviderInterface)
public:
	explicit InternetGuardPlugin(QObject* parent = nullptr);
	~InternetGuardPlugin();

	Plugin::Uid uid() const override
	{
		return Plugin::Uid{ QStringLiteral("a4b3c2d1-e5f6-7890-abcd-ef1234567890") };
	}

	QVersionNumber version() const override
	{
		return QVersionNumber(1, 2);
	}

	QString name() const override
	{
		return QStringLiteral("InternetGuard");
	}

	QString description() const override
	{
		return tr("Block or allow internet access on student computers");
	}

	QString vendor() const override
	{
		return QStringLiteral("prof. Ing. Raffaele Mele");
	}

	QString copyright() const override
	{
		return QStringLiteral("(C) 2025-26  prof. Ing. Raffaele Mele");
	}

	const FeatureList& featureList() const override
	{
		return m_features;
	}

	bool controlFeature(Feature::Uid featureUid, Operation operation,
	                    const QVariantMap& arguments,
	                    const ComputerControlInterfaceList& computerControlInterfaces) override;

	bool handleFeatureMessage(VeyonServerInterface& server,
	                          const MessageContext& messageContext,
	                          const FeatureMessage& message) override;

private:
	enum Commands
	{
		BlockInternetCommand,
		AllowInternetCommand
	};

	// True if the uid belongs to one of this plugin's features.
	bool isOwnFeature(Feature::Uid featureUid) const;

	// --- server-side actions (run on the student computer) ---
	// Thin logging wrappers around the WindowsFirewall backend (no-ops with a
	// warning on non-Windows platforms).
	static void blockInternet();
	static void allowInternet();

	// Internet toggle (Mode): one-click block/allow for the selected computers,
	// with explicit per-selection Block/Allow actions as sub-features (shown in
	// the toolbar dropdown and the right-click context menu).
	const Feature m_internetAccessFeature;
	const Feature m_blockInternetFeature;
	const Feature m_allowInternetFeature;

	const FeatureList m_features;
};
