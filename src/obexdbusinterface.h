#ifndef OBEXDBUSINTERFACE_H
#define OBEXDBUSINTERFACE_H

#include <QVariantMap>
#include <QtDBus/QDBusArgument>

#include <qmailid.h>

class QMailStore;
class QMailMessage;

class ObexDBusInterface : public QObject
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.sailfish.qmf.obex")
public:
    static const QString dbusService;
    static const QString dbusPath;
    explicit ObexDBusInterface(QObject *parent = 0);

    enum MAPEventType {
        NewMessage,
        DeliverySuccess,
        SendingSuccess,
        DeliveryFailure,
        SendingFailure,
        MemoryFull,
        MemoryAvailable,
        MessageDeleted,
        MessageShifted,
        ReadStatusChanged
    };

public slots:
    Q_SCRIPTABLE const QVariantList listAccounts() const;
    Q_SCRIPTABLE const QVariantList listFolders(const QString &account, const QString &folder, quint16 max, quint16 offset) const;
    Q_SCRIPTABLE const QVariantList listMessages(const QString &account, const QString &folder, quint16 max, quint16 offset, const QVariantMap &filter) const;

    Q_SCRIPTABLE const QVariantMap getMessage(qint64 id) const;
    Q_SCRIPTABLE qint64 putMessage(const QVariantMap data);
    Q_SCRIPTABLE int setMessage(qint64 id, quint8 indicator, bool value);

    Q_SCRIPTABLE int messageUpdate(const QString &account, const QString &folder);

signals:
    Q_SCRIPTABLE void mapEventReport(quint8 type, qint64 id, const QString &msg_type, const QVariantMap &kvargs) const;

protected slots:
    void notifyMessages(const QMailMessageIdList&, MAPEventType);

private slots:
    void messagesAdded(const QMailMessageIdList&);
    void messagesUpdated(const QMailMessageIdList&);
    void messagesRemoved(const QMailMessageIdList&);

private:
    QMailStore *_store;
    QList<QMailMessage*> _queue;
};

#endif // OBEXDBUSINTERFACE_H
