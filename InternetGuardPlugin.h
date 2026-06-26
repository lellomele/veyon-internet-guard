/*
 * InternetGuardPlugin.h
 * Copyright (C) 2025-26  prof. Ing. Raffaele Mele
 *
 * Third-party Veyon plugin — not part of the official Veyon distribution.
 */

#pragma once

#include <QList>
#include "VeyonCompat.h"

class InternetGuardPlugin : public QObject, FeatureProviderInterface, PluginInterface
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
		return QVersionNumber(1, 0);
	}

	QString name() const override
	{
		return QStringLiteral("InternetGuard");
	}

	QString description() const override
	{
		return tr("Block or allow internet access with a single toggle");
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

	static void runNetshBatch(const QList<QStringList>& batch);
	static void blockInternet();
	static void allowInternet();

	const Feature m_internetAccessFeature;
	const FeatureList m_features;
};
