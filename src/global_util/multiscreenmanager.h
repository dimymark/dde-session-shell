#ifndef MULTISCREENMANAGER_H
#define MULTISCREENMANAGER_H

#include <QObject>
#include <QWidget>
#include <QScreen>
#include <QMap>
#include <functional>
#include <QTimer>
#include "monitor.h"

#include <com_deepin_daemon_display.h>
#include <com_deepin_daemon_display_monitor.h>
using DisplayInter = com::deepin::daemon::Display;
using MonitorInter = com::deepin::daemon::display::Monitor;

class SessionBaseModel;

class MultiScreenManager : public QObject
{
    Q_OBJECT
public:
    explicit MultiScreenManager(QObject *parent = nullptr);
    void setSessionBaseModel(SessionBaseModel *model) {m_model = model;}
    void register_for_mutil_monitor(std::function<QWidget* (Monitor *)> function);
    void startRaiseContentFrame();
    void onMonitorsChanged(const QList<QDBusObjectPath> & mons);
    void hideAllFullScreen();
private:
    void raiseContentFrame();
    Monitor *monitorAdded(const QString &path);
    void monitorRemoved(const QString &path);

private:
    QTimer *m_raiseContentFrameTimer;
    std::function<QWidget* (Monitor *)> m_registerMonitorFun;
    QMap<Monitor*, QWidget*> m_frameMoniter;
    DisplayInter m_displayInter;
    SessionBaseModel *m_model = nullptr;
};

#endif // MULTISCREENMANAGER_H
