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

#include <DLog>

#include "src/dde-lock//lockframe.h"
#include "src/dde-lock/dbus/dbuslockfrontservice.h"
#include "src/dde-lock/dbus/dbuslockagent.h"
#include "src/global_util/multiscreenmanager.h"

#include "src/session-widgets/lockcontent.h"
#include "src/dde-lock/lockworker.h"
#include "src/session-widgets/sessionbasemodel.h"
#include "src/widgets/propertygroup.h"
#include "src/global_util/monitor.h"

#include <QLabel>
#include <QScreen>
#include <QWindow>
#include <dapplication.h>
#include <QDBusInterface>
#include <QDesktopWidget>
#include <DGuiApplicationHelper>
#include <QMovie>
#include <unistd.h>

DCORE_USE_NAMESPACE
DWIDGET_USE_NAMESPACE

int main(int argc, char *argv[])
{
    qputenv("QT_WAYLAND_SHELL_INTEGRATION", "kwayland-shell");
    DApplication app(argc, argv);
    //解决Qt在Retina屏幕上图片模糊问题
    QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    app.setOrganizationName("deepin");
    app.setApplicationName("dde-lock");
    app.setApplicationVersion("2015.1.0");

    DGuiApplicationHelper::instance()->setPaletteType(DGuiApplicationHelper::LightType);
    DPalette pa = DGuiApplicationHelper::instance()->applicationPalette();
    pa.setColor(QPalette::Normal, DPalette::WindowText, QColor("#FFFFFF"));
    pa.setColor(QPalette::Normal, DPalette::Text, QColor("#FFFFFF"));
    pa.setColor(QPalette::Normal, DPalette::AlternateBase, QColor(0, 0, 0, 76));
    pa.setColor(QPalette::Normal, DPalette::Button, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::Light, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::Dark, QColor(255, 255, 255, 76));
    pa.setColor(QPalette::Normal, DPalette::ButtonText, QColor("#FFFFFF"));
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::WindowText, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Text, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::AlternateBase, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Button, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Light, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::Dark, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::generatePaletteColor(pa, DPalette::ButtonText, DGuiApplicationHelper::LightType);
    DGuiApplicationHelper::instance()->setApplicationPalette(pa);

    DLogManager::registerConsoleAppender();
    DLogManager::registerFileAppender();

    QTranslator translator;
    translator.load("/usr/share/dde-session-shell/translations/dde-session-shell_" + QLocale::system().name());
    app.installTranslator(&translator);

    QCommandLineParser cmdParser;
    cmdParser.addHelpOption();
    cmdParser.addVersionOption();

    QCommandLineOption backend(QStringList() << "d" << "daemon", "start to daemon mode");
    cmdParser.addOption(backend);
    QCommandLineOption switchUser(QStringList() << "s" << "switch", "show user switch");
    cmdParser.addOption(switchUser);
    cmdParser.process(app);

    bool runDaemon = cmdParser.isSet(backend);
    bool showUserList = cmdParser.isSet(switchUser);

    SessionBaseModel *model = new SessionBaseModel(SessionBaseModel::AuthType::LockType);
    LockWorker *worker = new LockWorker(model); //
    PropertyGroup *property_group = new PropertyGroup(worker);
    qDebug() << "Supported animated file formats:" << QMovie::supportedFormats();

    property_group->addProperty("contentVisible");

    DBusLockAgent agent;
    agent.setModel(model);
    DBusLockFrontService service(&agent);

    auto createFrame = [&] (Monitor *monitor) -> QWidget* {
        LockFrame *lockFrame = new LockFrame(model);
        property_group->addObject(lockFrame);
        lockFrame->setMonitor(monitor);

        QObject::connect(lockFrame, &LockFrame::requestSwitchToUser, worker, &LockWorker::switchToUser);
        QObject::connect(lockFrame, &LockFrame::requestAuthUser, worker, &LockWorker::authUser);
        QObject::connect(model, &SessionBaseModel::visibleChanged, lockFrame, &LockFrame::visibleChangedFrame);
        QObject::connect(model, &SessionBaseModel::visibleChanged, lockFrame,[&](bool visible) {
            emit service.Visible(visible);
        });
        QObject::connect(model, &SessionBaseModel::showUserList, lockFrame, &LockFrame::showUserList);
        QObject::connect(lockFrame, &LockFrame::requestSetLayout, worker, &LockWorker::setLayout);
        QObject::connect(lockFrame, &LockFrame::requestEnableHotzone, worker, &LockWorker::enableZoneDetected, Qt::UniqueConnection);
        QObject::connect(lockFrame, &LockFrame::destroyed, property_group, &PropertyGroup::removeObject);
        QObject::connect(monitor, &Monitor::enableChanged, lockFrame, &LockFrame::monitorEnableChanged);


        qDebug() << "create lockframe and setVisible:" << lockFrame << (model->isShow() && monitor->enable());
        //lockFrame->setVisible(model->isShow() && monitor->enable()); //随后有同样的代码，而且区分深度与域管

        QObject::connect(lockFrame, &LockFrame::sendKeyValue, [&](QString key) {
             emit service.ChangKey(key);
        });

        if (isDeepinAuth()) {
            lockFrame->setVisible(model->isShow() && monitor->enable());
        } else {
            model->setIsShow(false);
            lockFrame->setVisible(false);
        }
        return lockFrame;
    };

    MultiScreenManager multi_screen_manager;
    multi_screen_manager.setSessionBaseModel(model);
    multi_screen_manager.register_for_mutil_monitor(createFrame);

    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::hideAllFullScreen);
    QObject::connect(model, &SessionBaseModel::visibleChanged, &multi_screen_manager, &MultiScreenManager::startRaiseContentFrame);

    QDBusConnection conn = QDBusConnection::sessionBus();
    if (!conn.registerService(DBUS_NAME) ||
            !conn.registerObject("/com/deepin/dde/lockFront", &agent) ||
            !app.setSingleInstance(QString("dde-lock%1").arg(getuid()), DApplication::UserScope)) {
        qDebug() << "register dbus failed"<< "maybe lockFront is running..." << conn.lastError();

        if (!runDaemon) {
            const char *interface = "com.deepin.dde.lockFront";
            QDBusInterface ifc(DBUS_NAME, DBUS_PATH, interface, QDBusConnection::sessionBus(), NULL);
            if (showUserList) {
                ifc.asyncCall("ShowUserList");
            } else {
                qDebug() << "show lockfram with dbus";
                ifc.asyncCall("Show");
            }
        }
    } else {
        if (isDeepinAuth() && !runDaemon) {
            if (showUserList) {
                emit model->showUserList();
            } else {
                qDebug() << "show lockfram first";
                model->setIsShow(true);
            }
        }
        app.exec();
    }

    return 0;
}
