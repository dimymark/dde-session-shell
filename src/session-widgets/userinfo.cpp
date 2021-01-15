#include "userinfo.h"

#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include "src/global_util/public_func.h"

#include <syslog.h>

#define LOCK_AUTH_NUM 5

static bool checkUserIsNoPWGrp(User const *user)
{
    if (user->type() == User::ADDomain) {
        return false;
    }

    // Caution: 32 here is unreliable, and there may be more
    // than this number of groups that the user joins.

    int ngroups = 32;
    gid_t groups[32];
    struct group *gr = nullptr;

    struct passwd pwd = {0};
    struct passwd *ppwd = NULL;
    char buf[4096] = {0};
    int ret = getpwnam_r(user->name().toUtf8().data(), &pwd, buf, sizeof(buf), &ppwd);
    if (ppwd == nullptr) {
        qDebug() << "fetch passwd structure failed, username: " << user->name();
        return false;
    }

    /* Retrieve group list */

    if (getgrouplist(user->name().toUtf8().data(), ppwd->pw_gid, groups, &ngroups) == -1) {
        fprintf(stderr, "getgrouplist() returned -1; ngroups = %d\n",
                ngroups);
        return false;
    }

    /* Display list of retrieved groups, along with group names */

    for (int i = 0; i < ngroups; i++) {
        gr = getgrgid(groups[i]);
        if (gr != nullptr && QString(gr->gr_name) == QString("nopasswdlogin")) {
            return true;
        }
    }

    return false;
}

static const QString toLocalFile(const QString &path)
{
    QUrl url(path);

    if (url.isLocalFile()) {
        return url.path();
    }

    return url.url();
}

User::User(QObject *parent)
    : QObject(parent)
    , m_isLogind(false)
    , m_isLock(false)
    , m_lockNum(4)
    , m_tryNum(5)
    , m_locale(getenv("LANG"))
    , m_lockTimer(new QTimer)
{
    m_lockTimer->setInterval(1000 * 60);
    m_lockTimer->setSingleShot(false);
    connect(m_lockTimer.get(), &QTimer::timeout, this, &User::onLockTimeOut);
}

User::User(const User &user)
    : QObject(user.parent())
    , m_isLogind(user.m_isLogind)
    , m_isLock(user.m_isLock)
    , m_uid(user.m_uid)
    , m_lockNum(user.m_lockNum)
    , m_tryNum(user.m_tryNum)
    , m_userName(user.m_userName)
    , m_locale(user.m_locale)
    , m_lockTimer(user.m_lockTimer)
{

}

User::~User()
{
    userInfoRecordOperate(false);
}

bool User::operator==(const User &user) const
{
    return type() == user.type() &&
           m_uid == user.m_uid;
}

void User::setLocale(const QString &locale)
{
    if (m_locale == locale) return;

    m_locale = locale;

    emit localeChanged(locale);
}

bool User::isNoPasswdGrp() const
{
    return checkUserIsNoPWGrp(this);
}

bool User::isUserIsvalid() const
{
    return true;
}

void User::setisLogind(bool isLogind)
{
    if (m_isLogind == isLogind) {
        return;
    }

    m_isLogind = isLogind;

    emit logindChanged(isLogind);
}

void User::setPath(const QString &path)
{
    if (m_path == path) return;

    m_path = path;
}

bool User::isLockForNum()
{
    syslog(LOG_INFO, "zl: %s %d is lock %d try num %d ", __func__, __LINE__, m_isLock, m_tryNum);
    return m_isLock || --m_tryNum == 0;
}

void User::startLock(bool is_start)
{
    if (is_start) m_startTime = time(nullptr);//切换到其他用户时，由于Qtimer自身机制导致无法进入timeout事件，导致被锁定的账户不能继续执行，解决bug4511

    if (m_lockTimer->isActive()) return;

    m_isLock = true;

    onLockTimeOut();
}

void User::resetLock()
{
    m_tryNum = 5;
}

void User::onLockTimeOut()
{
    int min = (time(nullptr) - m_startTime) / 60;
    if (min >= 3) {
        m_lockTimer->stop();
        m_tryNum = 5;
        m_lockNum = 4;
        m_isLock = false;
        m_startTime = 0;
    } else if (min >= 2) {
        m_lockNum = 1;
        m_lockTimer->start();
    } else if (min >= 1) {
        m_lockNum = 2;
        m_lockTimer->start();
    } else if (min == 0){
        m_lockNum = 3;
        m_lockTimer->start();

        QJsonObject userValue;
        userValue.insert("isLock", m_isLock);
        userValue.insert("startTime", int(time(nullptr)));

        userInfoRecordOperate(true, userValue);
    } else {
        qDebug() << "Time is negative, can't be here!";
    }


    emit lockChanged(m_tryNum == 0);
}

/**
 * @brief 检查用户账户本地存储信息
 */
void User::checkUserInfo()
{
    QFile openFile("/var/lib/lightdm/1.json");
    if(!openFile.open(QIODevice::ReadOnly)) {
        qDebug() << "File open error";
        return ;
    } else {
        QString strUid = QString::number(m_uid);
        QByteArray allData = openFile.readAll();
        QJsonDocument jsonDocRead(QJsonDocument::fromJson(allData));
        openFile.close();
        QJsonObject rootObj = jsonDocRead.object();
        if (rootObj.contains(strUid)) {
            QJsonValue jsonValue = rootObj.value(strUid);
            if (jsonValue.isObject()) {
                QJsonObject usrValue = jsonValue.toObject();
                m_isLock = usrValue["isLock"].toBool();
                m_startTime = time_t(usrValue["startTime"].toInt());
                if (m_isLock) {
                    m_tryNum = 0;
                    startLock(false);
                }
            }
        }
    }
}

/**
 * @brief 用户记录信息增删操作
 */
void User::userInfoRecordOperate(bool add, const QJsonObject &value)
{
    QFile file("/var/lib/lightdm/1.json");
    if(!file.open( QIODevice::ReadWrite)) {
        qDebug() << "File open error";
        return;
    }
    QString strUid = QString::number(m_uid);
    QJsonParseError Jsonerror;
    QByteArray allData = file.readAll();
    QJsonDocument jsonDoc(QJsonDocument::fromJson(allData, &Jsonerror));
    qDebug() << Jsonerror.errorString();
    QJsonObject jsonObject = jsonDoc.object();

    if (add)
        jsonObject.insert(strUid, value);
    else
        jsonObject.remove(strUid);

    jsonDoc.setObject(jsonObject);

    file.resize(0);
    file.write(jsonDoc.toJson());
    file.close();
}

NativeUser::NativeUser(const QString &path, QObject *parent)
    : User(parent)
    , m_userInter(new UserInter(ACCOUNT_DBUS_SERVICE, path, QDBusConnection::systemBus(), this))
{
    connect(m_userInter, &UserInter::IconFileChanged, this, &NativeUser::avatarChanged);
    connect(m_userInter, &UserInter::FullNameChanged, this, [ = ](const QString & fullname) {
        m_fullName = fullname;
        emit displayNameChanged(fullname.isEmpty() ? m_userName : fullname);
    });
    connect(m_userInter, &UserInter::UserNameChanged, this, [ = ](const QString & user_name) {
        m_userName = user_name;
        emit displayNameChanged(m_fullName.isEmpty() ? m_userName : m_fullName);
    });

    connect(m_userInter, &UserInter::DesktopBackgroundsChanged, this, [ = ] {
        emit desktopBackgroundPathChanged(desktopBackgroundPath());
    });

    connect(m_userInter, &UserInter::GreeterBackgroundChanged, this, [ = ](const QString & path) {
        emit greeterBackgroundPathChanged(toLocalFile(path));
    });

    connect(m_userInter, &UserInter::LocaleChanged, this, &NativeUser::setLocale);
    connect(m_userInter, &UserInter::HistoryLayoutChanged, this, &NativeUser::kbLayoutListChanged);
    connect(m_userInter, &UserInter::LayoutChanged, this, &NativeUser::currentKBLayoutChanged);
    connect(m_userInter, &UserInter::NoPasswdLoginChanged, this, &NativeUser::noPasswdLoginChanged);
    connect(m_userInter, &UserInter::Use24HourFormatChanged, this, &NativeUser::use24HourFormatChanged);

    m_userName = m_userInter->userName();
    m_uid = m_userInter->uid().toInt();
    m_locale = m_userInter->locale();

    setPath(path);
    checkUserInfo();
}

void NativeUser::setCurrentLayout(const QString &layout)
{
    m_userInter->SetLayout(layout);
}

QString NativeUser::displayName() const
{
    const QString &fullname = m_userInter->fullName();
    return fullname.isEmpty() ? name() : fullname;
}

QString NativeUser::avatarPath() const
{
    return m_userInter->iconFile();
}

QString NativeUser::greeterBackgroundPath() const
{
    //mark sp2的dde-preload取背景方式还不稳定，先回退到sp1
    return toLocalFile(m_userInter->greeterBackground());
}

QString NativeUser::desktopBackgroundPath() const
{
    return toLocalFile(m_userInter->desktopBackgrounds().first());
}

QStringList NativeUser::kbLayoutList()
{
    return m_userInter->historyLayout();
}

QString NativeUser::currentKBLayout()
{
    return m_userInter->layout();
}

bool NativeUser::isNoPasswdGrp() const
{
    return (m_userInter->passwordStatus() == "NP" || checkUserIsNoPWGrp(this));
}

bool NativeUser::isPasswordExpired() const
{
    QDBusPendingReply<bool> replay = m_userInter->IsPasswordExpired();
    replay.waitForFinished();
    return !replay.isError() && m_userInter->IsPasswordExpired();
}

bool NativeUser::isUserIsvalid() const
{
    //无效用户的时候m_userInter是有效的
    return m_userInter->isValid() && !m_userName.isEmpty();
}

bool NativeUser::is24HourFormat() const
{
    if(!isUserIsvalid()) m_userInter->use24HourFormat();

    return true;
}

ADDomainUser::ADDomainUser(uid_t uid, QObject *parent)
    : User(parent)
{
    m_uid = uid;
    checkUserInfo();
}

void ADDomainUser::setUserDisplayName(const QString &name)
{
    if (m_displayName == name) {
        return;
    }

    m_displayName = name;

    emit displayNameChanged(name);
}

void ADDomainUser::setUserName(const QString &name)
{
    if (m_userName == name) {
        return;
    }

    m_userName = name;
}

void ADDomainUser::setUserInter(UserInter *user_inter)
{
    if (m_userInter == user_inter) {
        return;
    }

    m_userInter = user_inter;
}

void ADDomainUser::setUid(uid_t uid)
{
    if (m_uid == uid) {
        return;
    }

    m_uid = uid;
}

void ADDomainUser::setIsServerUser(bool is_server)
{
    if (m_isServer == is_server) {
        return;
    }

    m_isServer = is_server;
}

QString ADDomainUser::displayName() const
{
    return m_displayName.isEmpty() ? m_userName : m_displayName;
}

QString ADDomainUser::avatarPath() const
{
    return QString(":/img/default_avatar.svg");
}

QString ADDomainUser::greeterBackgroundPath() const
{
    return QString("/usr/share/wallpapers/deepin/desktop.jpg");
}

QString ADDomainUser::desktopBackgroundPath() const
{
    return QString("/usr/share/wallpapers/deepin/desktop.jpg");
}

bool ADDomainUser::isPasswordExpired() const
{
    if (m_userInter != nullptr) {
        QDBusPendingReply<bool> replay = m_userInter->IsPasswordExpired();
        replay.waitForFinished();
        return !replay.isError() && m_userInter->IsPasswordExpired();
    }
    return false;
}
