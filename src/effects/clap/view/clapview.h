/*
 * Audacity: A Digital Audio Editor
 */
#pragma once

#include <memory>
#include <unordered_map>

#include <QQuickItem>
#include <QPointer>
#include <qqmlintegration.h>

#include "global/modularity/ioc.h"

#include "effects/effects_base/ieffectinstancesregister.h"

#include "au3-clap/IClapHostListener.h"

class ClapInstance;
class QTimer;
class QSocketNotifier;
class QWindow;

namespace au::effects {
//! QQuickItem hosting a CLAP plugin's native GUI inside an embedded QWindow.
//! Implements IClapHostListener so the plugin's clap_host_gui /
//! clap_host_timer_support / clap_host_posix_fd_support callbacks land here.
class ClapView : public QQuickItem, public IClapHostListener
{
    Q_OBJECT

    Q_PROPERTY(int instanceId READ instanceId WRITE setInstanceId NOTIFY instanceIdChanged FINAL)
    Q_PROPERTY(QString title READ title NOTIFY titleChanged FINAL)
    Q_PROPERTY(int sidePadding READ sidePadding WRITE setSidePadding NOTIFY sidePaddingChanged FINAL)
    Q_PROPERTY(int topPadding READ topPadding WRITE setTopPadding NOTIFY topPaddingChanged FINAL)
    Q_PROPERTY(int bottomPadding READ bottomPadding WRITE setBottomPadding NOTIFY bottomPaddingChanged FINAL)
    Q_PROPERTY(int minimumWidth READ minimumWidth WRITE setMinimumWidth NOTIFY minimumWidthChanged FINAL)

    QML_ELEMENT

    muse::GlobalInject<IEffectInstancesRegister> instancesRegister;

public:
    explicit ClapView(QQuickItem* parent = nullptr);
    ~ClapView() override;

    Q_INVOKABLE void init();
    Q_INVOKABLE void deinit();

    int instanceId() const { return m_instanceId; }
    void setInstanceId(int id);
    QString title() const { return m_title; }
    int sidePadding() const { return m_sidePadding; }
    void setSidePadding(int v);
    int topPadding() const { return m_topPadding; }
    void setTopPadding(int v);
    int bottomPadding() const { return m_bottomPadding; }
    void setBottomPadding(int v);
    int minimumWidth() const { return m_minimumWidth; }
    void setMinimumWidth(int v);

    // IClapHostListener
    bool registerTimer(uint32_t periodMs, uint32_t& outTimerId) override;
    bool unregisterTimer(uint32_t timerId) override;
    bool registerFd(int fd, uint32_t flags) override;
    bool modifyFd(int fd, uint32_t flags) override;
    bool unregisterFd(int fd) override;
    bool guiRequestResize(uint32_t width, uint32_t height) override;
    bool guiRequestShow() override;
    bool guiRequestHide() override;
    void guiClosed(bool wasDestroyed) override;
    void guiResizeHintsChanged() override;

signals:
    void instanceIdChanged();
    void titleChanged();
    void sidePaddingChanged();
    void topPaddingChanged();
    void bottomPaddingChanged();
    void minimumWidthChanged();

private:
    static const char* currentPlatformApi();
    void updateViewGeometry();
    void destroyGuiAndWindow();
    void attachFdNotifier(int fd, uint32_t flags);

    int m_instanceId = -1;
    //! Strong ref to the CLAP instance for the lifetime of the view.
    std::shared_ptr<::EffectInstanceEx> m_instanceHolder;
    ClapInstance* m_instance = nullptr;
    QWindow* m_clapWindow = nullptr;
    bool m_guiCreated = false;
    QString m_title;
    int m_sidePadding = 0;
    int m_topPadding = 0;
    int m_bottomPadding = 0;
    int m_minimumWidth = 0;

    std::unordered_map<uint32_t, QTimer*> m_timers;
    uint32_t m_nextTimerId = 1;

    struct FdEntry {
        std::unique_ptr<QSocketNotifier> read;
        std::unique_ptr<QSocketNotifier> write;
        std::unique_ptr<QSocketNotifier> error;
    };
    std::unordered_map<int, FdEntry> m_fds;
};
}
