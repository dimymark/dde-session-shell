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

#include "lockframe.h"
#include "src/session-widgets/lockcontent.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/session-widgets/userinfo.h"
#include "src/session-widgets/hibernatewidget.h"

#include <QApplication>
#include <QWindow>

#include <syslog.h>



LockFrame::LockFrame(SessionBaseModel *const model, QWidget *parent)
    : FullscreenBackground(parent)
    , m_model(model)
{

    this->setWindowFlag(Qt::WindowMinMaxButtonsHint, false);

    QTimer::singleShot(0, this, [ = ] {
        auto user = model->currentUser();
        if (user != nullptr) updateBackground(user->greeterBackgroundPath());
    });

    Hibernate = new HibernateWidget(this);
    Hibernate->hide();
    m_content = new LockContent(model, this);
    m_content->setVisible(false);
    setContent(m_content);
    connect(m_content, &LockContent::requestSwitchToUser, this, &LockFrame::requestSwitchToUser);
    connect(m_content, &LockContent::requestAuthUser, this, &LockFrame::requestAuthUser);
    connect(m_content, &LockContent::requestSetLayout, this, &LockFrame::requestSetLayout);
    connect(m_content, &LockContent::requestBackground, this, static_cast<void (LockFrame::*)(const QString &)>(&LockFrame::updateBackground));
    connect(model, &SessionBaseModel::blackModeChanged, this, &FullscreenBackground::setIsBlackMode);
    connect(model, &SessionBaseModel::HibernateModeChanged, this, [&](bool is_hibernate){
        if(is_hibernate){
            setContent(Hibernate);
            setIsHibernateMode();     //更新大小 现示动画
        }else{
            Hibernate->hide();
            setContent(m_content);
        }
    });
    connect(model, &SessionBaseModel::showUserList, this, &LockFrame::showUserList);
    connect(m_content, &LockContent::unlockActionFinish,this, [ = ]() {
        Q_EMIT requestEnableHotzone(true);
        m_model->setIsShow(false);
    });
    connect(model, &SessionBaseModel::authFinished, this, [ = ](bool success){
        syslog(LOG_INFO, "zl: SessionBaseModel::authFinished %d -- success status %d: ", __LINE__, success);
        qDebug() << "SessionBaseModel::authFinished -- success status : " << success;
        m_content->beforeUnlockAction(success);
    });
}

bool LockFrame::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyRelease) {
        QString  keyValue = "";
        switch (static_cast<QKeyEvent *>(event)->key()) {
        case Qt::Key_NumLock: {
            keyValue = "numlock";
            break;
        }
        case Qt::Key_TouchpadOn: {
            keyValue = "touchpad-on";
            break;
        }
        case Qt::Key_TouchpadOff: {
            keyValue = "touchpad-off";
            break;
        }
        case Qt::Key_TouchpadToggle: {
            keyValue = "touchpad-toggle";
            break;
        }
        case Qt::Key_CapsLock: {
            keyValue = "capslock";
            break;
        }
        case Qt::Key_VolumeDown: {
            keyValue = "audio-lower-volume";
            break;
        }
        case Qt::Key_VolumeUp: {
            keyValue = "audio-raise-volume";
            break;
        }
        case Qt::Key_VolumeMute: {
            keyValue = "audio-mute";
            break;
        }
        case Qt::Key_MonBrightnessUp: {
            keyValue = "mon-brightness-up";
            break;
        }
        case Qt::Key_MonBrightnessDown: {
            keyValue = "mon-brightness-down";
            break;
        }
        }

        if (keyValue != "") {
            emit sendKeyValue(keyValue);
        }
    }
    return FullscreenBackground::eventFilter(watched, event);
}

void LockFrame::enterEvent(QEvent *event)
{
    Q_UNUSED(event);
    qDebug() << "LockFrame::enterEvent: set content visible true";
    setContentVisible(true);
}

void LockFrame::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    //setContentVisible(false); //多数情况下不允许主动hide，除非是要求所有都隐藏的情况
}

void LockFrame::showUserList()
{
    show();
    m_model->setCurrentModeState(SessionBaseModel::ModeStatus::UserMode);
}

void LockFrame::visibleChangedFrame(bool isVisible)
{
    QDBusInterface *inter1 = nullptr;
    if (qEnvironmentVariable("XDG_SESSION_TYPE").toLower().contains("wayland")) {
        inter1 = new QDBusInterface("org.kde.KWin", "/KWin", "org.kde.KWin",
                                                  QDBusConnection::sessionBus(), this);
    }

    if (inter1) {
        auto req1 = inter1->call("disableHotKeysForClient", isVisible);
    }
    QDBusInterface launcherInter("com.deepin.dde.Launcher", "/com/deepin/dde/Launcher", "com.deepin.dde.Launcher"
                                 , QDBusConnection::sessionBus());
    launcherInter.call("Hide");

    qDebug() << "LockFrame::visibleChangedFrame:" << this << (isVisible && m_monitor->enable());
    if (isVisible && m_monitor->enable()) {
        updateMonitorGeometry();
    //     show();
    } else {
    //     setVisible(false);
    }
}

void LockFrame::monitorEnableChanged(bool isEnable)
{
    qDebug() << "LockFrame::monitorEnableChanged:" << m_monitor->name() << isEnable;
    this->setVisible(isEnable && m_model->isShow());
}

void LockFrame::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
#ifdef QT_DEBUG
    case Qt::Key_Escape:    qApp->quit();       break;
#endif
    }
}

void LockFrame::showEvent(QShowEvent *event)
{
    emit requestEnableHotzone(false);

    m_model->setIsShow(true);

    return FullscreenBackground::showEvent(event);
}

void LockFrame::hideEvent(QHideEvent *event)
{
    emit requestEnableHotzone(true);

    return FullscreenBackground::hideEvent(event);
}

LockFrame::~LockFrame()
{
    //+ 防止插拔HDMI显示屏出现崩溃问题，需要析构时调用对应delete释放资源；
    delete m_content;
}
