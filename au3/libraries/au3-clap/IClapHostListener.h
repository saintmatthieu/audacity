/**********************************************************************

  Audacity: A Digital Audio Editor

  @file IClapHostListener.h

  @brief SDK-free interface for the muse-side host of the CLAP host extensions
  (timer-support, posix-fd-support, gui). ClapView (Qt-side) implements this and
  registers itself on the ClapInstance; ClapWrapper forwards plugin->host callbacks
  here.

**********************************************************************/
#pragma once

#include <cstdint>

namespace ClapWindowApi {
//! Mirrors the CLAP_WINDOW_API_* string constants (SDK-free for src consumers).
constexpr const char* kWin32 = "win32";
constexpr const char* kCocoa = "cocoa";
constexpr const char* kX11 = "x11";
constexpr const char* kWayland = "wayland";
}

class IClapHostListener
{
public:
    virtual ~IClapHostListener() = default;

    // clap.timer-support host side
    //! Plugin asks the host to register a periodic timer. Returns true and sets
    //! \p outTimerId on success; \p outTimerId must be a unique value the host
    //! will pass back to the plugin via on_timer().
    virtual bool registerTimer(uint32_t periodMs, uint32_t& outTimerId) = 0;
    virtual bool unregisterTimer(uint32_t timerId) = 0;

    // clap.posix-fd-support host side
    //! POSIX fd polling flags (mirror CLAP_POSIX_FD_READ/WRITE/ERROR).
    enum FdFlags : uint32_t {
        FdRead = 1u << 0,
        FdWrite = 1u << 1,
        FdError = 1u << 2,
    };
    virtual bool registerFd(int fd, uint32_t flags) = 0;
    virtual bool modifyFd(int fd, uint32_t flags) = 0;
    virtual bool unregisterFd(int fd) = 0;

    // clap.gui host side
    virtual bool guiRequestResize(uint32_t width, uint32_t height) = 0;
    virtual bool guiRequestShow() = 0;
    virtual bool guiRequestHide() = 0;
    //! Plugin tells us its (typically floating) window was closed.
    virtual void guiClosed(bool wasDestroyed) = 0;
    virtual void guiResizeHintsChanged() = 0;
};
