/*
 * Copyright (C) 2015 ~ 2018 Deepin Technology Co., Ltd.
 *
 * Author:     sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * Maintainer: sbw <sbw@sbw.so>
 *             kirigaya <kirigaya@mkacg.com>
 *             Hualet <mr.asianwang@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LOCKFRAME
#define LOCKFRAME

#include "src/widgets/fullscreenbackground.h"
#include "src/session-widgets/hibernatewidget.h"

#include <QKeyEvent>
#include <QDBusConnection>
#include <QDBusAbstractAdaptor>
#include <memory>

const QString DBUS_LOCK_PATH = "/com/deepin/dde/lockFront";
const QString DBUS_LOCK_NAME = "com.deepin.dde.lockFront";

const QString DBUS_SHUTDOWN_PATH = "/com/deepin/dde/shutdownFront";
const QString DBUS_SHUTDOWN_NAME = "com.deepin.dde.shutdownFront";

class DBusLockService;
class SessionBaseModel;
class LockContent;
class WarningContent;
class User;
class LockFrame: public FullscreenBackground
{
    Q_OBJECT
public:
    LockFrame(SessionBaseModel *const model, QWidget *parent = nullptr);
    ~LockFrame() override;

signals:
    void requestAuthUser(const QString &password);
    void requestSwitchToUser(std::shared_ptr<User> user);
    void requestSetLayout(std::shared_ptr<User> user, const QString &layout);
    void requestEnableHotzone(bool disable);
    void sendKeyValue(QString keyValue);

public slots:
    void showUserList();
    void showShutdown();
    void shutdownInhibit(const SessionBaseModel::PowerAction action);
    void cancelShutdownInhibit();

protected:
    void keyPressEvent(QKeyEvent *e) Q_DECL_OVERRIDE;
    void showEvent(QShowEvent *event) Q_DECL_OVERRIDE;
    void hideEvent(QHideEvent *event) Q_DECL_OVERRIDE;
    bool event(QEvent *event) Q_DECL_OVERRIDE;

private:
    bool handlePoweroffKey();

private:
    LockContent *m_lockContent;
    WarningContent *m_warningContent = nullptr;
    SessionBaseModel *m_model;
    bool m_preparingSleep;
    bool m_prePreparingSleep;
    long m_preparingSleepTime;
    QCursor m_oldCursor;
};

#endif // LOCKFRAME

