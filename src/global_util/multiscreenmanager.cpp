#include "multiscreenmanager.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QDebug>
#include <QDesktopWidget>

#include <com_deepin_daemon_display.h>
using DisplayInter=com::deepin::daemon::Display;

MultiScreenManager::MultiScreenManager(QObject *parent)
    : QObject(parent)
    , m_raiseContentFrameTimer(new QTimer(this))
    , m_displayInter("com.deepin.daemon.Display", "/com/deepin/daemon/Display", QDBusConnection::sessionBus(), this)
{
    connect(&m_displayInter, &DisplayInter::MonitorsChanged, this, &MultiScreenManager::onMonitorsChanged);

    // 在sw平台存在复制模式显示问题，使用延迟来置顶一个Frame
    m_raiseContentFrameTimer->setInterval(80);
    m_raiseContentFrameTimer->setSingleShot(true);

    connect(m_raiseContentFrameTimer, &QTimer::timeout, this, &MultiScreenManager::raiseContentFrame);
}

void MultiScreenManager::startRaiseContentFrame()
{
    m_raiseContentFrameTimer->start();
}

void MultiScreenManager::register_for_mutil_monitor(std::function<QWidget* (Monitor *)> function)
{
    QList<QDBusObjectPath> monitor = m_displayInter.property("Monitors").value<QList<QDBusObjectPath>>();
    m_registerMonitorFun = function;
    onMonitorsChanged(monitor);
}

void MultiScreenManager::raiseContentFrame()
{
    for (auto it = m_frameMoniter.constBegin(); it != m_frameMoniter.constEnd(); ++it) {
        if (it.value()->property("contentVisible").toBool()) {
            it.value()->raise();
            return;
        }
    }
}

void MultiScreenManager::onMonitorsChanged(const QList<QDBusObjectPath> & mons)
{
    QList<QString> ops;
    for (const auto *mon : m_frameMoniter.keys())
        ops << mon->path();

    qDebug() << mons.size();
    QList<QString> pathList;
    QList<Monitor *> monitors;
    Monitor *primaryMonitor = nullptr;
    for (const auto op : mons) {
        const QString path = op.path();
        pathList << path;
        qDebug() << path;
        if (!ops.contains(path)) {
           Monitor *monitor = monitorAdded(path);
           // 主屏队列后移
            if (monitor->isPrimary()) {
                primaryMonitor = monitor;
                continue;
            }
            monitors << monitor;
        }
    }
    if(primaryMonitor != nullptr)
         monitors << primaryMonitor;

    // 主屏最后new，保证多屏复制模式下显示在最上面，wayland环境下，widget->raise()函数不生效
    for (Monitor *mon : monitors) {
        m_frameMoniter[mon] = m_registerMonitorFun(mon);
        startRaiseContentFrame();
    }

    for (const auto op : ops)
        if (!pathList.contains(op))
            monitorRemoved(op);
}

Monitor *MultiScreenManager::monitorAdded(const QString &path)
{
    MonitorInter *inter = new MonitorInter("com.deepin.daemon.Display", path, QDBusConnection::sessionBus(), this);
    Monitor *mon = new Monitor(this);
    connect(inter, &MonitorInter::XChanged, mon, &Monitor::setX);
    connect(inter, &MonitorInter::YChanged, mon, &Monitor::setY);
    connect(inter, &MonitorInter::WidthChanged, mon, &Monitor::setW);
    connect(inter, &MonitorInter::HeightChanged, mon, &Monitor::setH);
    connect(inter, &MonitorInter::EnabledChanged, mon, &Monitor::setMonitorEnable);
    connect(inter, &MonitorInter::ModesChanged, mon, &Monitor::setMonitorModes);
    // NOTE: DO NOT using async dbus call. because we need to have a unique name to distinguish each monitor
    Q_ASSERT(inter->isValid());
    mon->setName(inter->name());
    mon->setMonitorEnable(inter->enabled());
    mon->setPath(path);
    mon->setX(inter->x());
    mon->setY(inter->y());
    mon->setW(inter->width());
    mon->setH(inter->height());
    mon->setMonitorModes(inter->modes());
    mon->setPrimary(m_displayInter.primary());

    return mon;
}

void MultiScreenManager::monitorRemoved(const QString &path)
{
    Monitor *monitor = nullptr;
    for (auto it(m_frameMoniter.cbegin()); it != m_frameMoniter.cend(); ++it) {
        if (it.key()->path() == path) {
            monitor = it.key();
            break;
        }
    }
    if (!monitor)
        return;
    m_frameMoniter[monitor]->deleteLater();
    m_frameMoniter.remove(monitor);
    monitor->deleteLater();
    startRaiseContentFrame();
}
