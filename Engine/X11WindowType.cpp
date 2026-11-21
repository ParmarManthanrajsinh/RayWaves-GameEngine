#include "WindowUtils.h"

// Kept in its own translation unit: raylib redefines Xlib's Font type, so this
// must never include raylib headers.

#if defined(__linux__)

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <unistd.h>

namespace WindowUtils
{
void SetNormalWindowType()
{
    Display* display = XOpenDisplay(nullptr);
    if (display == nullptr)
    {
        return;
    }

    const ::Window root = DefaultRootWindow(display);
    const Atom pid_property = XInternAtom(display, "_NET_WM_PID", True);
    const Atom type_property = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    const Atom normal_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE_NORMAL", False);
    const long process_id = static_cast<long>(getpid());

    ::Window root_return = 0;
    ::Window parent_return = 0;
    ::Window* children = nullptr;
    unsigned int child_count = 0;
    if (XQueryTree(display, root, &root_return, &parent_return, &children, &child_count) == 0)
    {
        XCloseDisplay(display);
        return;
    }

    // GLFW creates the window before it maps it, so the top-level window for
    // this process exists here but is still hidden from the compositor.
    for (unsigned int index = 0; index < child_count; ++index)
    {
        Atom actual_type = None;
        int actual_format = 0;
        unsigned long item_count = 0;
        unsigned long bytes_after = 0;
        unsigned char* data = nullptr;
        const int status = XGetWindowProperty(display, children[index], pid_property, 0, 1, False, XA_CARDINAL,
            &actual_type, &actual_format, &item_count, &bytes_after, &data);

        const bool b_same_process = status == Success && data != nullptr && item_count == 1 && actual_format == 32 &&
            *reinterpret_cast<long*>(data) == process_id;
        if (data != nullptr)
        {
            XFree(data);
        }
        if (!b_same_process)
        {
            continue;
        }

        XChangeProperty(display, children[index], type_property, XA_ATOM, 32, PropModeReplace,
            reinterpret_cast<const unsigned char*>(&normal_type), 1);

        // Hyprland floats XWayland windows when the size hints do not contain
        // real min/max values: it normalizes missing bounds to -1, and then
        // min == max. Give it explicit, different bounds so the editor tiles.
        // This also keeps GLFW's position/size hints from making it float.
        XSizeHints hints{};
        hints.flags = PMinSize | PMaxSize | PWinGravity;
        hints.min_width = 320;
        hints.min_height = 240;
        hints.max_width = 32767;
        hints.max_height = 32767;
        hints.win_gravity = StaticGravity;
        XSetWMNormalHints(display, children[index], &hints);
        break;
    }

    if (children != nullptr)
    {
        XFree(children);
    }
    XFlush(display);
    XCloseDisplay(display);
}
} // namespace WindowUtils

#else

namespace WindowUtils
{
void SetNormalWindowType()
{
}
} // namespace WindowUtils

#endif
