#include "obexdbusinterface.h"

#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMetaType>

#include <qmailstore.h>
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
            ret.append(item);
        }
    }
    return ret;
}

const QVariantList ObexDBusInterface::listMessages(const QString &account, const QString &folder, quint16 max, quint16 offset, const QVariantMap &filter) const
{
    QVariantList ret;
    QMailMessageKey mmk;
    QMailMessageSortKey msk;
    QMailFolderIdList fil = _store->queryFolders(QMailFolderKey::path(folder));
    QMailMessageId qmi;
    QMailMessageIdList qml;
    if(folder.isEmpty()) {
        qDebug() << "Cannot list messages at root. Did you mean INBOX?";
        return ret;
    }
    if(fil.isEmpty()) {
        qDebug() << "Nof folder " << folder << " found.";
        return ret;
    }
    mmk = QMailMessageKey::parentFolderId(fil.at(0));
    if(!account.isEmpty()) {
        QMailAccountIdList mal = _store->queryAccounts(QMailAccountKey::name(account, QMailDataComparator::Includes));
        if(mal.isEmpty()) {
            qDebug() << "No account containing " << account << " found";
            return ret;
        }
        mmk &= QMailMessageKey::parentAccountId(mal);
    }
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
        foreach (qmi, qml) {
            QVariantMap item;
            QMailMessage qmm = _store->message(qmi);
            item.insert("id",qmi.toULongLong());
            item.insert("type",msgType(qmm.messageType()));
            item.insert("subject",qmm.subject());
            item.insert("datetime",qmm.date().toString());
            item.insert("sender_name",qmm.from().name());
            item.insert("sender_addressing",qmm.from().address());
            item.insert("replyto_addressing",qmm.replyTo().address());
            item.insert("recipient_name",qmm.recipients().at(0).name());
            item.insert("recipient_addressing",qmm.recipients().at(0).address());
            item.insert("size", qmm.indicativeSize());
            item.insert("text",qmm.hasBody()?"yes":"no");
            item.insert("reception_status",qmm.contentAvailable()?"complete":(qmm.partialContentAvailable()?"fractioned":"notification"));
            item.insert("attachment_size",qmm.hasAttachments()?qmm.size():0);
            item.insert("priority",(qmm.status()&QMailMessage::HighPriority)?"yes":"no");
            item.insert("sent", (qmm.status()&QMailMessage::Sent)?"yes":"mo");
            item.insert("read", (qmm.status()&QMailMessage::Read)?"yes":"no");
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
    QMailMessageBody body = QMailMessageBody::fromData(data.value("body").toString(),QMailMessageContentType("text/plain"),QMailMessageBody::EightBit);

    qmm->setBody(body);
    qmm->setSubject(data.value("subject").toString());
    qmm->setTo(QMailAddress(data.value("to").toString()));
    qmm->setDate(QMailTimeStamp(data.value("datetime").toDateTime()));
    qmm->setStatus(QMailMessage::Outbox | QMailMessage::Draft, true);
    qmm->setStatus(QMailMessage::Outgoing, true);
    qmm->setStatus(QMailMessage::ContentAvailable, true);
    // TODO: more fields
    if(data.contains("account")) {
        mal = _store->queryAccounts(QMailAccountKey::name(data.value("account").toString()));
    } else {
        mal = _store->queryAccounts(QMailAccountKey::fromAddress(data.value("from").toString()));
    }
    if(mal.isEmpty()) {
        qDebug() << "Cannot identify mail account neither from " << data.value("account") << " nor from " << data.value("from");
        return -1;
    }
    qmm->setParentAccountId(mal.at(0));
    fil = _store->queryFolders(QMailFolderKey::path(data.value("folder","outbox").toString()) & QMailFolderKey::parentAccountId(mal.at(0)));
    if(fil.isEmpty()) {
        qDebug() << "Cannot find folder for account ID " << mal.at(0).toULongLong() << " " << data.value("folder");
        return -2;
    }
    qmm->setParentFolderId(fil.at(0));
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
    QMailMessage qmm = _store->message(QMailMessageId(id));
    QMailStorageAction *msa;
    if(!qmm.id().isValid()) {
        qDebug() << "Cannot find message with ID " << id;
        return -1;
    }
    msa = new QMailStorageAction(this);
    connect(msa, &QMailStorageAction::activityChanged, [=](QMailStorageAction::Activity a) {
        if(a == QMailRetrievalAction::Successful || a == QMailRetrievalAction::Failed) {
            msa->deleteLater();
            qDebug() << "Removal complete " << ((a==QMailRetrievalAction::Successful)?"successfully":"with error");
            if(a == QMailStorageAction::Successful) {
                QMailRetrievalAction *mra = new QMailRetrievalAction(this);
                connect(mra,&QMailRetrievalAction::activityChanged, [=](QMailRetrievalAction::Activity a){
                    if(a == QMailRetrievalAction::Successful || a == QMailRetrievalAction::Failed) {
                        mra->deleteLater();
                        qDebug() << "Symc complete " << ((a==QMailRetrievalAction::Successful)?"successfully":"with error");
                    }
                });
                mra->exportUpdates(qmm.parentAccountId());
            }
        }
    });
    if(indicator == 0) {
        qmm.setStatus(QMailMessage::Read, value);
        msa->updateMessages(QMailMessageList() << qmm);
    } else if(indicator == 1) {
        if(value && qmm.status()&QMailMessage::Removed) {
            // irreversible delete of deleted
            msa->deleteMessages(QMailMessageIdList() << qmm.id());
        } else if(value) {
            // reversible recyle bin removal
            msa->moveToStandardFolder(QMailMessageIdList() << qmm.id(), QMailFolder::TrashFolder);
        } else {
            // undelete
            msa->restoreToPreviousFolder(QMailMessageKey::id(qmm.id()));
        }
    } else {
        qDebug() << "Unsupported indicator value " << indicator;
        delete msa;
        return -2;
    }
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
                qDebug() << "Update complete";
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
