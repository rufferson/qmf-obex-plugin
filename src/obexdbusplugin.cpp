#include "obexdbusplugin.h"

#include <qmaillog.h>
#include <QDebug>

ObexDbusPlugin::ObexDbusPlugin(QObject *parent):
    QMailMessageServerPlugin(parent)
{
    qDebug() << "DBus Plugin Initialisation";
    _service = new ObexDBusInterface(this);
}

ObexDbusPlugin::~ObexDbusPlugin()
{
}

void ObexDbusPlugin::exec()
{
    qMailLog(Messaging) << "DBus Plugin Execution";
    qDebug() << "DBus Plugin Execution";
}

QString ObexDbusPlugin::key() const
{
    return "obexdbus";
}

ObexDbusPlugin* ObexDbusPlugin::createService()
{
    return this;
}
