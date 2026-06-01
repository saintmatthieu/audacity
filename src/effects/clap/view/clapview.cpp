/*
 * Audacity: A Digital Audio Editor
 */
#include "clapview.h"

#include <QQuickWindow>
#include <QSocketNotifier>
#include <QTimer>
#include <QWindow>

#include "au3-clap/ClapInstance.h"

#include "log.h"

using namespace au::effects;

const char* ClapView::currentPlatformApi()
{
#if defined(Q_OS_WIN)
    return ClapWindowApi::kWin32;
#elif defined(Q_OS_MAC)
    return ClapWindowApi::kCocoa;
#else
    return ClapWindowApi::kX11;
#endif
}

ClapView::ClapView(QQuickItem* parent)
    : QQuickItem(parent)
{
}

ClapView::~ClapView()
{
    deinit();
}

void ClapView::setInstanceId(int id)
{
    if (m_instanceId == id) {
        return;
    }
    m_instanceId = id;
    emit instanceIdChanged();
}

void ClapView::setSidePadding(int v)
{
    if (m_sidePadding == v) {
        return;
    }
    m_sidePadding = v;
    emit sidePaddingChanged();
    updateViewGeometry();
}

void ClapView::setTopPadding(int v)
{
    if (m_topPadding == v) {
        return;
    }
    m_topPadding = v;
    emit topPaddingChanged();
    updateViewGeometry();
}

void ClapView::setBottomPadding(int v)
{
    if (m_bottomPadding == v) {
        return;
    }
    m_bottomPadding = v;
    emit bottomPaddingChanged();
    updateViewGeometry();
}

void ClapView::setMinimumWidth(int v)
{
    if (m_minimumWidth == v) {
        return;
    }
    m_minimumWidth = v;
    emit minimumWidthChanged();
    updateViewGeometry();
}

void ClapView::init()
{
    if (m_instanceId < 0 || m_guiCreated) {
        return;
    }

    m_instanceHolder = instancesRegister()->instanceById(m_instanceId);
    if (!m_instanceHolder) {
        LOGE() << "ClapView::init: no instance for id=" << m_instanceId;
        return;
    }
    m_instance = dynamic_cast<ClapInstance*>(m_instanceHolder.get());
    if (!m_instance) {
        LOGE() << "ClapView::init: instance " << m_instanceId << " is not a ClapInstance";
        m_instanceHolder.reset();
        return;
    }

    if (!m_instance->hasGui()) {
        // Plugin advertises no clap.gui extension; leave the viewer empty.
        return;
    }

    const char* api = currentPlatformApi();
    if (!m_instance->guiIsApiSupported(api, /*isFloating=*/ false)) {
        LOGW() << "ClapView::init: plugin does not support api '" << api << "' (embedded)";
        return;
    }

    // The plugin can call back into host_gui / timer-support / posix-fd-support
    // any time after create(); register the listener first.
    m_instance->setHostListener(this);

    if (!m_instance->guiCreate(api, /*isFloating=*/ false)) {
        LOGE() << "ClapView::init: gui.create failed";
        m_instance->setHostListener(nullptr);
        m_instance = nullptr;
        m_instanceHolder.reset();
        return;
    }
    m_guiCreated = true;

#if defined(Q_OS_WIN)
    if (window()) {
        m_instance->guiSetScale(window()->devicePixelRatio());
    }
#endif

    // Create the child native window the plugin will be embedded into.
    m_clapWindow = new QWindow(window());

    void* handle = reinterpret_cast<void*>(m_clapWindow->winId());
    if (!m_instance->guiSetParent(api, handle)) {
        LOGE() << "ClapView::init: gui.set_parent failed";
        destroyGuiAndWindow();
        return;
    }

    updateViewGeometry();

    m_clapWindow->show();
    m_instance->guiShow();
}

void ClapView::deinit()
{
    destroyGuiAndWindow();

    for (auto& [id, t] : m_timers) {
        t->stop();
        t->deleteLater();
    }
    m_timers.clear();
    m_fds.clear();

    // destroyGuiAndWindow() above already cleared the listener IFF we were
    // still it; don't re-clear here (that would clobber a newer ClapView that
    // has since taken over the same plugin instance).
    m_instance = nullptr;
    m_instanceHolder.reset();
}

void ClapView::destroyGuiAndWindow()
{
    // Only touch the plugin's GUI state if we are still the active listener.
    // When the user closes one dialog and quickly opens another for the same
    // realtime effect instance, a NEW ClapView's init() races ahead of the
    // OLD ClapView's destructor and re-establishes the plugin GUI. The OLD
    // ClapView must not then call guiDestroy() on the (shared) plugin, or it
    // would tear down the NEW ClapView's freshly-created widgets.
    if (m_instance && m_instance->hostListener() == this) {
        if (m_guiCreated) {
            m_instance->guiHide();
            m_instance->guiDestroy();
        }
        m_instance->setHostListener(nullptr);
    }
    m_guiCreated = false;
    if (m_clapWindow) {
        m_clapWindow->hide();
        delete m_clapWindow;
        m_clapWindow = nullptr;
    }
}

void ClapView::updateViewGeometry()
{
    if (!m_instance || !m_clapWindow || !m_guiCreated) {
        return;
    }

    uint32_t w = 0, h = 0;
    if (!m_instance->guiGetSize(w, h) || w == 0 || h == 0) {
        return;
    }

    const int implicitW = std::max(m_minimumWidth, static_cast<int>(w));
    setImplicitWidth(implicitW);
    setImplicitHeight(static_cast<int>(h));

    const int sidePadding = std::max(m_sidePadding, (implicitW - static_cast<int>(w)) / 2);
    m_clapWindow->setGeometry(sidePadding, m_topPadding, static_cast<int>(w), static_cast<int>(h));
}

// IClapHostListener -----------------------------------------------------------

bool ClapView::registerTimer(uint32_t periodMs, uint32_t& outTimerId)
{
    const uint32_t id = m_nextTimerId++;
    auto* timer = new QTimer(this);
    timer->setInterval(static_cast<int>(std::max<uint32_t>(periodMs, 1u)));
    connect(timer, &QTimer::timeout, this, [this, id]() {
        if (m_instance) {
            m_instance->fireTimer(id);
        }
    });
    timer->start();
    m_timers.emplace(id, timer);
    outTimerId = id;
    return true;
}

bool ClapView::unregisterTimer(uint32_t timerId)
{
    auto it = m_timers.find(timerId);
    if (it == m_timers.end()) {
        return false;
    }
    it->second->stop();
    it->second->deleteLater();
    m_timers.erase(it);
    return true;
}

void ClapView::attachFdNotifier(int fd, uint32_t flags)
{
    auto& entry = m_fds[fd];
    entry.read.reset();
    entry.write.reset();
    entry.error.reset();

    auto bindNotifier = [&](QSocketNotifier::Type type, uint32_t flag) -> std::unique_ptr<QSocketNotifier> {
        if (!(flags & flag)) {
            return nullptr;
        }
        auto n = std::make_unique<QSocketNotifier>(fd, type, this);
        connect(n.get(), &QSocketNotifier::activated, this, [this, fd, flag]() {
            if (m_instance) {
                m_instance->fireFd(fd, flag);
            }
        });
        return n;
    };

    entry.read = bindNotifier(QSocketNotifier::Read, IClapHostListener::FdRead);
    entry.write = bindNotifier(QSocketNotifier::Write, IClapHostListener::FdWrite);
    entry.error = bindNotifier(QSocketNotifier::Exception, IClapHostListener::FdError);
}

bool ClapView::registerFd(int fd, uint32_t flags)
{
    if (m_fds.count(fd)) {
        return false;
    }
    attachFdNotifier(fd, flags);
    return true;
}

bool ClapView::modifyFd(int fd, uint32_t flags)
{
    if (!m_fds.count(fd)) {
        return false;
    }
    attachFdNotifier(fd, flags);
    return true;
}

bool ClapView::unregisterFd(int fd)
{
    return m_fds.erase(fd) > 0;
}

bool ClapView::guiRequestResize(uint32_t width, uint32_t height)
{
    if (!m_clapWindow) {
        return false;
    }
    const int implicitW = std::max(m_minimumWidth, static_cast<int>(width));
    setImplicitWidth(implicitW);
    setImplicitHeight(static_cast<int>(height));
    const int sidePadding = std::max(m_sidePadding, (implicitW - static_cast<int>(width)) / 2);
    m_clapWindow->setGeometry(sidePadding, m_topPadding,
                              static_cast<int>(width), static_cast<int>(height));
    return true;
}

bool ClapView::guiRequestShow()
{
    if (m_clapWindow) {
        m_clapWindow->show();
    }
    return true;
}

bool ClapView::guiRequestHide()
{
    if (m_clapWindow) {
        m_clapWindow->hide();
    }
    return true;
}

void ClapView::guiClosed(bool /*wasDestroyed*/)
{
    // Floating-window close notification; embedded mode shouldn't normally fire this.
    destroyGuiAndWindow();
}

void ClapView::guiResizeHintsChanged()
{
    // We don't currently expose resize hints to the user; the next host-initiated
    // resize will refetch via adjust_size.
}
