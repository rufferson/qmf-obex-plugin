#ifndef OBEXDBUSINTERFACE_H
#define OBEXDBUSINTERFACE_H

#include <QVariantMap>
#include <QtDBus/QDBusArgument>
#include <QList>

#include <qmailid.h>

class QMailStore;
class QMailMessage;
class QMailMessageKey;
Q_DECLARE_METATYPE(QList<qint64>)

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
    enum FilterParamMask {
        Subject          = 0x000001,    // 0
        DateTime         = 0x000002,    // 1
        SenderName       = 0x000004,    // 2
        SenderAddressing = 0x000008,    // 3
        RecipientName    = 0x000010,    // 4
        RecipientAddressing = 0x000020, // 5
        Type             = 0x000040,    // 6
        Size             = 0x000080,    // 7
        ReceptionStatus  = 0x000100,    // 8
        Text             = 0x000200,    // 9
        AttachmentSize   = 0x000400,    // 10
        Priority         = 0x000800,    // 11
        Read             = 0x001000,    // 12
        Sent             = 0x002000,    // 13
        Protected        = 0x004000,    // 14
        ReplyToAddressing= 0x008000,    // 15
        DeliveryStatus   = 0x010000,    // 16
        ConversationId   = 0x020000,    // 17
        ConversationName = 0x040000,    // 18
        Direction        = 0x080000,    // 19
        AttachmentMime   = 0x100000,    // 20
        Reserved         = 0x200000     // 21-31
    };
    Q_DECLARE_FLAGS(MaskParams, FilterParamMask)

public slots:
    Q_SCRIPTABLE const QVariantList listAccounts() const;
    Q_SCRIPTABLE const QVariantList listFolders(const QString &account, const QString &folder, quint16 max, quint16 offset) const;
    Q_SCRIPTABLE const QVariantList listThreads(const QString &account, const QString &folder, quint16 max, quint16 offset) const;
    Q_SCRIPTABLE const QList<qint64> listMessages(const QString &account, const QString &folder, quint16 max, quint16 offset, const QVariantMap &filter) const;

    Q_SCRIPTABLE const QVariantMap getMetadata(qint64, quint32 mask) const;
    Q_SCRIPTABLE const QVariantMap getMessage(qint64 id, quint32 flags) const;
    Q_SCRIPTABLE qint64 putMessage(const QVariantMap data, quint32 flags);
    Q_SCRIPTABLE int setMessage(qint64 id, quint8 indicator, bool value);

    Q_SCRIPTABLE int messageUpdate(const QString &account, const QString &folder, int min);

signals:
    Q_SCRIPTABLE void mapEventReport(quint8 type, qint64 id, const QString &msg_type, const QVariantMap &kvargs) const;

protected slots:
    void notifyMessages(const QMailMessageIdList&, MAPEventType);

private slots:
    void messagesAdded(const QMailMessageIdList&);
    void messagesUpdated(const QMailMessageIdList&);
    void messagesRemoved(const QMailMessageIdList&);

    void collectListing(const QMailMessageIdList&,quint32,QVariantList&);

private:
    const QMailMessageKey prepareMessagesFilter(const QString &account, const QString &folder, const QVariantMap &filter) const;
    const QMailThreadIdList queryThreads(const QString &account, const QString &folder, quint16 max, quint16 offset) const;
    const QVariantMap buildConversation(const QMailThreadId &mti) const;

    QMailStore *_store;
    QList<QMailMessage*> _queue;
};

#endif // OBEXDBUSINTERFACE_H
