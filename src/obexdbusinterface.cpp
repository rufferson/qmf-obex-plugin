#include "obexdbusinterface.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMetaType>

#include <qmailstore.h>
#include <qmaildisconnected.h>
#include <qmailserviceaction.h>

#include <QDebug>

const QString ObexDBusInterface::dbusService = "org.sailfish.qmf.obex";
const QString ObexDBusInterface::dbusPath = "/org/sailfish/qmf/obex";

ObexDBusInterface::ObexDBusInterface(QObject *parent) : QObject(parent)
{
    QDBusConnection dbusSession(QDBusConnection::sessionBus());

    dbusSession.registerService(dbusService);
    dbusSession.registerObject(dbusPath, this,
        QDBusConnection::ExportScriptableSlots|QDBusConnection::ExportScriptableSignals);

    _store = QMailStore::instance();
    connect(_store, SIGNAL(messagesAdded(const QMailMessageIdList&)), SLOT(messagesAdded(QMailMessageIdList)));
    connect(_store, SIGNAL(messagesUpdated(const QMailMessageIdList&)), SLOT(messagesUpdated(QMailMessageIdList)));
    connect(_store, SIGNAL(messagesRemoved(const QMailMessageIdList&)), SLOT(messagesRemoved(QMailMessageIdList)));
}

static const char* msgType(QMailMessage::MessageType type)
{
    switch(type) {
    case QMailMessage::Mms:
        return "MMS";
    case QMailMessage::Sms:
        return "SMS_GSM";
    case QMailMessage::Email:
        return "EMAIL";
    case QMailMessage::Instant:
        return "IM";
    case QMailMessage::AnyType:
        return "ANY";
    default:
        return "OTHER";
    }
}

void ObexDBusInterface::notifyMessages(const QMailMessageIdList &ids, MAPEventType type)
{
    QMailMessageId qmi;
    foreach (qmi, ids) {
        QMailMessage qmm = _store->message(qmi);
        QVariantMap args;
        args.insert("folder",_store->folder(qmm.parentFolderId()).path());
        if(qmm.previousParentFolderId().isValid())
            args.insert("old_folder",_store->folder(qmm.previousParentFolderId()).path());
        if(type == NewMessage) {
            args.insert("datetime",qmm.date().toString());
            args.insert("subject", qmm.subject());
            args.insert("sender_name",qmm.from().name());
            args.insert("priority",(qmm.status()&QMailMessage::HighPriority)?"yes":"no");
        }
        emit mapEventReport(type, qmi.toULongLong(),msgType(qmm.messageType()),args);
    }
}

void ObexDBusInterface::messagesAdded(const QMailMessageIdList &ids)
{
    QMailMessage *qmm;
    if(!_queue.isEmpty()) {
        foreach (qmm, _queue) {
            if(ids.contains(qmm->id())) {
                qDebug() << "Added ID " << qmm->id().toULongLong() << " was added by us";
                return;
            }
        }
    }
    notifyMessages(ids,NewMessage);
}

void ObexDBusInterface::messagesUpdated(const QMailMessageIdList &ids)
{
    QMailMessage *qmm;
    if(!_queue.isEmpty()) {
        foreach (qmm, _queue) {
            if(ids.contains(qmm->id())) {
                qDebug() << "Modified ID " << qmm->id().toULongLong() << " was added by us";
                return;
            }
        }
    }
    qDebug() << "Modified events: " << ids;
    // TODO: how to track changes??? Let's do it elsewhere
}

void ObexDBusInterface::messagesRemoved(const QMailMessageIdList &ids)
{
    QMailMessage *qmm;
    if(!_queue.isEmpty()) {
        foreach (qmm, _queue) {
            if(ids.contains(qmm->id())) {
                qDebug() << "Added ID " << qmm->id().toULongLong() << " was added by us";
                return;
            }
        }
    }
    notifyMessages(ids,MessageDeleted);
}

const QVariantList ObexDBusInterface::listFolders(const QString &account, const QString &folder, quint16 max, quint16 offset) const
{
    QVariantList ret;
    QMailFolderKey mfk;
    QMailFolderId mfi;
    QMailFolderIdList fil = _store->queryFolders(QMailFolderKey::path(folder));
    if(!folder.isEmpty() && fil.isEmpty())
        return ret;
    mfk = QMailFolderKey::parentFolderId( folder.isEmpty() ? QMailFolderId(0) : fil.at(0));
    if(!account.isEmpty()) {
        QMailAccountIdList mal = _store->queryAccounts(QMailAccountKey::name(account, QMailDataComparator::Includes));
        if(mal.isEmpty()) {
            qDebug() << "No account containing " << account << " found";
            return ret;
        }
        mfk &= QMailFolderKey::parentAccountId(mal);
    }
    qDebug() << "Listing " << max << " folders in " << folder << " from " << offset << " for " << account;
    if(max == 0) {
        int cnt = _store->countFolders(mfk);
        ret.append(cnt);
    } else {
        fil = _store->queryFolders(mfk, QMailFolderSortKey::path(),max,offset);
        foreach (mfi, fil) {
            QMailFolder qmf = _store->folder(mfi);
            QVariantMap item;
            item.insert("path",qmf.path());
            item.insert("name",qmf.displayName());
            item.insert("count", qmf.serverCount());
            item.insert("unread",qmf.serverUnreadCount());
            item.insert("account",_store->account(qmf.parentAccountId()).name());
            ret.append(item);
        }
    }
    return ret;
}

const QVariantList ObexDBusInterface::listMessages(const QString &account, const QString &folder, quint16 max, quint16 offset, const QVariantMap &filter) const
{
    QVariantList ret;
    QMailMessageKey mmk;
    QMailMessageSortKey msk = QMailMessageSortKey::timeStamp(Qt::DescendingOrder) & QMailMessageSortKey::receptionTimeStamp(Qt::DescendingOrder);
    QMailFolderKey mfk;
    QMailFolderIdList fil;
    QMailAccountIdList mal;
    QMailMessageIdList qml;
    QMailMessageId qmi;

    if(folder.isEmpty()) {
        qDebug() << "Cannot list messages at root. Did you mean INBOX?";
        return ret;
    }
    if(!account.isEmpty()) {
        mal = _store->queryAccounts(QMailAccountKey::name(account));
        if(mal.isEmpty()) {
            qDebug() << "No account containing " << account << " found";
            return ret;
        }
    } else
        mal = _store->queryAccounts();
    mfk = QMailFolderKey::parentAccountId(mal);
    fil = _store->queryFolders(mfk & QMailFolderKey::path(folder));
    if(fil.isEmpty() && folder.toLower() == "deleted")
        fil = _store->queryFolders(mfk & QMailFolderKey::path("trash"));
    if(fil.isEmpty()) {
        qDebug() << "No folder " << folder << " found.";
        return ret;
    }
    mmk = QMailMessageKey::parentAccountId(mal) & QMailMessageKey::parentFolderId(fil);
    qDebug() << "Listing " << max << " messages in " << folder << " from " << offset << " found folders: " << fil.length();
    if(filter.values().length()) {
        if(filter.value("type").toInt()>0)
            mmk &= QMailMessageKey::messageType(filter.value("type").toInt(),QMailDataComparator::Excludes);
        if(filter.value("read").toInt()>0)
            mmk &= QMailMessageKey::status(QMailMessage::Read,
                            (filter.value("read").toInt()==1) ? QMailDataComparator::Excludes : QMailDataComparator::Includes);
        if(filter.value("priority").toInt()>0)
            mmk &= QMailMessageKey::status(QMailMessage::HighPriority,
                            (filter.value("priority").toInt()==1) ? QMailDataComparator::Includes : QMailDataComparator::Excludes);
        if(filter.value("begin").toDateTime().isValid())
            mmk &= QMailMessageKey::timeStamp(filter.value("begin").toDateTime(),QMailDataComparator::GreaterThanEqual);
        if(filter.value("end").toDateTime().isValid())
            mmk &= QMailMessageKey::timeStamp(filter.value("end").toDateTime(),QMailDataComparator::LessThanEqual);
        if(!filter.value("from").toString().isEmpty())
            mmk &= QMailMessageKey::sender(filter.value("from").toString(),QMailDataComparator::Includes);
        if(!filter.value("rcpt").toString().isEmpty())
            mmk &= QMailMessageKey::recipients(filter.value("rcpt").toString(),QMailDataComparator::Includes);
    }
    if(max == 0) {
        int cnt = _store->countMessages(mmk);
        ret.append(cnt);
    } else {
        qml = _store->queryMessages(mmk,msk,max,offset);
        quint32 mask = filter.value("mask",(Subject | DateTime | RecipientAddressing | Type | Size | ReceptionStatus | AttachmentSize | ConversationId | Direction)).toULongLong();
        foreach (qmi, qml) {
            QVariantMap item;
            QMailMessage qmm = _store->message(qmi);
            item.insert("account", _store->account(qmm.parentAccountId()).name());
            item.insert("id",qmi.toULongLong());
            if(mask & Subject)
                item.insert("subject",qmm.subject());
            if(mask & DateTime)
                item.insert("datetime",qmm.date().toString());
            if(mask&SenderName)
                item.insert("sender_name",qmm.from().name());
            if(mask&SenderAddressing)
                item.insert("sender_addressing",qmm.from().address());
            if(mask&ReplyToAddressing)
                item.insert("replyto_addressing",qmm.replyTo().address());
            if(mask&RecipientName)
                item.insert("recipient_name",qmm.recipients().isEmpty()? "": qmm.recipients().at(0).name());
            if(mask&RecipientAddressing)
                item.insert("recipient_addressing",qmm.recipients().isEmpty()? "": qmm.recipients().at(0).address());
            if(mask&Type)
                item.insert("type",msgType(qmm.messageType()));
            if(mask&Size)
                item.insert("size", qmm.indicativeSize()*1024);
            if(mask&Text)
                item.insert("text",qmm.hasBody()?"yes":"no");
            if(mask&ReceptionStatus)
                item.insert("reception_status",qmm.contentAvailable()?"complete":(qmm.partialContentAvailable()?"fractioned":"notification"));
            if(mask&AttachmentSize)
                item.insert("attachment_size",qmm.hasAttachments()?qmm.size():0);
            if(mask&Priority)
                item.insert("priority", (qmm.status()&QMailMessage::HighPriority)?"yes":"no");
            if(mask&Read)
                item.insert("read",     (qmm.status()&QMailMessage::Read)        ?"yes":"no");
            if(mask&Sent)
                item.insert("sent",     (qmm.status()&QMailMessage::Sent)        ?"yes":"no");
            if(mask&Protected)
                item.insert("protected",QString("no"));
            if(mask&DeliveryStatus)
                item.insert("delivery_status", qmm.status()&QMailMessage::Sent ? "sent":"unknown");
            if(mask&ConversationId)
                item.insert("conversation_id",qmm.parentThreadId().toULongLong());
            if(mask&ConversationName)
                item.insert("conversation_name", _store->thread(qmm.parentThreadId()).subject());
            if(mask&Direction)
                item.insert("direction", (qmm.status()&QMailMessage::Outgoing)?"outgoing":"incoming");
            if(mask&AttachmentMime) {
                QList<QMailMessagePartContainer::Location> pll = qmm.findAttachmentLocations();
                QMailMessagePartContainer::Location pli;
                QStringList mpl;
                foreach (pli, pll) {
                    mpl.append(qmm.partAt(pli).contentType().toString(false,false));
                }
                item.insert("attachment_mime_types",mpl.join(","));
            }
            ret.append(item);
        }
    }
    return ret;
}

const QVariantMap ObexDBusInterface::getMessage(qint64 id) const
{
    QVariantMap ret;
    QMailMessageId mid((quint64)id);
    QMailMessage qmm = _store->message(mid);
    QMailFolder qmf;
    QMailAccount qma;
    if(!qmm.id().isValid()) {
        qDebug() << "No such message with id " << id;
        return ret;
    }
    qDebug() << "Fetching message " << mid.toULongLong();
    qmf = _store->folder(qmm.parentFolderId());
    qma = _store->account(qmm.parentAccountId());
    ret.insert("date",qmm.date().toString());
    ret.insert("subject",qmm.subject());
    ret.insert("from-n",qmm.from().name());
    ret.insert("from-mail",qmm.from().address());
    ret.insert("from-tel",qmm.from().minimalPhoneNumber());
    if(qmm.to().length()>0) {
        QMailAddress qma;
        QVariantList tos;
        foreach (qma, qmm.to()) {
            QVariantMap to;
            to.insert("n",qma.name());
            to.insert("mail",qma.address());
            to.insert("tel",qma.minimalPhoneNumber());
            tos.append(to);
        }
        ret.insert("to",tos);
    }
    ret.insert("type",msgType(qmm.messageType()));
    ret.insert("read",(bool)(qmm.status() & QMailMessage::Read));
    ret.insert("sent",(bool)(qmm.status() & QMailMessage::Sent));
    ret.insert("draft",(bool)(qmm.status() & QMailMessage::Draft));
    ret.insert("outbox",(bool)(qmm.status() & QMailMessage::Outbox));
    ret.insert("priority",(bool)(qmm.status() & QMailMessage::HighPriority));
    ret.insert("thread",qmm.parentThreadId().toULongLong());
    ret.insert("folder",qmf.path());
    ret.insert("account",qma.name());
    ret.insert("length", qmm.body().length());
    ret.insert("body",qmm.body().data(QMailMessageBody::Decoded));
    return ret;
}
// sqlite3 uses signed 64-bit integers.
qint64 ObexDBusInterface::putMessage(const QVariantMap data)
{
    QMailMessageId qmi;
    QMailTransmitAction *mta;
    QMailMessage *qmm = new QMailMessage();
    QMailAccountIdList mal;
    QMailFolderIdList fil;
    QMailFolderId fid;
    QMailMessageBody body = QMailMessageBody::fromData(data.value("body").toString(),QMailMessageContentType("text/plain"),QMailMessageBody::EightBit);

    qmm->setBody(body);
    qmm->setSubject(data.value("subject").toString());
    qmm->setTo(QMailAddress(data.value("to").toString()));
    qmm->setDate(QMailTimeStamp(data.value("datetime",QDateTime::currentDateTimeUtc()).toDateTime()));
    qmm->setStatus(QMailMessage::Outbox | QMailMessage::Draft, true);
    qmm->setStatus(QMailMessage::Outgoing, true);
    qmm->setStatus(QMailMessage::ContentAvailable, true);
    // TODO: more fields
    if(data.contains("account")) {
        mal = _store->queryAccounts(QMailAccountKey::name(data.value("account").toString()));
    } else if(data.contains("from")) {
        mal = _store->queryAccounts(QMailAccountKey::fromAddress(data.value("from").toString()));
    } else {
        mal = _store->queryAccounts(QMailAccountKey::status(QMailAccount::PreferredSender));
    }
    if(mal.isEmpty()) {
        qDebug() << "Cannot identify mail account neither from " << data.value("account") << " nor from " << data.value("from");
        return -1;
    }
    qmm->setParentAccountId(mal.at(0));
    fil = _store->queryFolders(QMailFolderKey::path(data.value("folder","outbox").toString()) & QMailFolderKey::parentAccountId(mal.at(0)));
    if(fil.isEmpty()) {
        fid = _store->account(mal.at(0)).standardFolder(QMailFolder::OutboxFolder);
        if(!fid.isValid())
            fid = _store->account(mal.at(0)).standardFolder(QMailFolder::DraftsFolder);
    } else
        fid = fil.at(0);
    if(!fid.isValid()) {
        qDebug() << "Cannot find folder for account ID " << mal.at(0).toULongLong() << " " << data.value("folder");
        return -2;
    }
    qmm->setParentFolderId(fid);
    // Store to outbox
    _queue.append(qmm);
    if(!_store->addMessage(qmm)) {
        qDebug() << "Cannot store message for delivery";
        _queue.removeAll(qmm);
        delete qmm;
        return -3;
    }
    qmi = qmm->id();
    _queue.removeAll(qmm);
    delete qmm;
    // Now transmit the message
    mta = new QMailTransmitAction(this);
    connect(mta, &QMailTransmitAction::messagesTransmitted, [=](const QMailMessageIdList &ids){
        mta->deleteLater();
        qDebug() << "Successful transmission of the " << ids;
        notifyMessages(ids, SendingSuccess);
    });
    connect(mta, &QMailTransmitAction::messagesFailedTransmission, [=](const QMailMessageIdList &ids, QMailServiceAction::Status::ErrorCode err){
        mta->deleteLater();
        qDebug() << "Failed transmission of the " << ids << err;
        notifyMessages(ids, SendingFailure);
    });
    mta->transmitMessage(qmi);
    return (qint64)qmi.toULongLong();
}

int ObexDBusInterface::setMessage(qint64 id, quint8 indicator, bool value)
{
    int ret = 0;
    QMailMessageMetaData mmd = _store->messageMetaData(QMailMessageId(id));
    if(!mmd.id().isValid()) {
        qDebug() << "Cannot find message with ID " << id;
        return -1;
    }
    if(indicator == 0) {
        qDebug() << "Setting Read state to " << value;
        if(!_store->updateMessagesMetaData(QMailMessageKey::id(mmd.id()), QMailMessage::Read, value)) {
            qDebug() << "Message state change failed in local storage";
            return -4;
        }

    } else if(indicator == 1) {
        qDebug() << "Removal action: " << value;
        if(value && mmd.status()&QMailMessage::Removed) {
            // irreversible delete of deleted
            if(_store->removeMessages(QMailMessageKey::id(mmd.id()), QMailStore::CreateRemovalRecord)) {
                qDebug() << "Message removal failed in local storage";
                return -3;
            }
        } else if(value) {
            // reversible recyle bin removal
            QMailDisconnected::moveToStandardFolder(QMailMessageIdList() << mmd.id(), QMailFolder::TrashFolder);
        } else {
            // undelete
            QMailDisconnected::restoreToPreviousFolder(QMailMessageKey::id(mmd.id()));
        }
    } else {
        qDebug() << "Unsupported indicator value " << indicator;
        return -2;
    }
    QMailRetrievalAction *mra = new QMailRetrievalAction(this);
    connect(mra,&QMailRetrievalAction::activityChanged, [=](QMailRetrievalAction::Activity a){
        if(a == QMailRetrievalAction::Successful || a == QMailRetrievalAction::Failed) {
            mra->deleteLater();
            qDebug() << "Remote Sync complete " << ((a==QMailRetrievalAction::Successful)?"successfully":"with error");
        }
    });
    mra->exportUpdates(mmd.parentAccountId());
    return ret;
}

int ObexDBusInterface::messageUpdate(const QString &account, const QString &folder)
{
    QMailAccountKey mak = account.isEmpty() ? QMailAccountKey() : QMailAccountKey::name(account);
    QMailAccountIdList mal = _store->queryAccounts(mak);
    QMailAccountId mai;
    foreach (mai, mal) {
        QMailRetrievalAction *sync;
        QMailFolderIdList fil;
        if(!folder.isEmpty()) {
            fil = _store->queryFolders(QMailFolderKey::parentAccountId(mai) & QMailFolderKey::path(folder));
            if(fil.isEmpty()) {
                qDebug() << "No folder " << folder << " found for account " << account;
                return -1;
            }
        }
        sync = new QMailRetrievalAction(this);
        connect(sync, &QMailRetrievalAction::activityChanged, [=](QMailServiceAction::Activity a){
            if(a == QMailServiceAction::Successful || a == QMailSearchAction::Failed) {
                sync->deleteLater();
                qDebug() << "Update complete: " << a << "/" << sync->status().errorCode << ":" << sync->status().text;
            }
        });
        qDebug() << "Requesting update for folder " << fil << " at " << mai.toULongLong();
        sync->retrieveNewMessages(mai, fil);
    }
    return 0;
}

const QVariantList ObexDBusInterface::listAccounts() const
{
    QMailAccountIdList mal = _store->queryAccounts();
    QMailAccountId mai;
    QVariantList ret;
    foreach (mai, mal) {
        QMailAccount qma = _store->account(mai);
        QVariantMap item;
        item.insert("name",qma.name());
        item.insert("address",qma.fromAddress().address());
        item.insert("mtime",qma.lastSynchronized().toString());
        item.insert("mtype",msgType(qma.messageType()));
        ret.append(item);
    }
    return ret;
}
