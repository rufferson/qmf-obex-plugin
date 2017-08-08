#ifndef ObexDbusPlugin_H
#define ObexDbusPlugin_H

#include "obexdbusinterface.h"

#include <qmailmessageserverplugin.h>
#include <QtPlugin>

class ObexDbusPlugin : public QMailMessageServerPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.ObexDbusPluginHandlerFactoryInterface")
public:
    ObexDbusPlugin(QObject *parent = 0);
    ~ObexDbusPlugin();

    virtual QString key() const;
    virtual void exec();
    virtual ObexDbusPlugin* createService();

private:
    ObexDBusInterface *_service;
};

#endif // ObexDbusPlugin_H
