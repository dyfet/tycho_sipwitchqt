#include <QtCore/QString>
#include <QtGui/QIcon>
#include <AppKit/NSDockTile.h>
#include <AppKit/NSApplication.h>
#include <AppKit/NSImage.h>
#include <Foundation/NSString.h>
#include <Foundation/NSProcessInfo.h>
#include "../Common/types.hpp"

void set_dock_label(const UString& text)
{
    NSString *string = [[[NSString alloc] initWithUTF8String:text.constData()] autorelease];
    [[NSApp dockTile] setBadgeLabel:string];
}

void set_dock_icon(const QIcon& icon)
{
    QPixmap pix = icon.pixmap(QSize(64, 64));
    QImage img = pix.toImage();
    CGImageRef cgi = img.toCGImage();

    NSImage * image = [[[NSImage alloc] initWithCGImage:cgi size:NSZeroSize] autorelease];
    [NSApp setApplicationIconImage: image];
}

void disable_nap()
{
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&  MAC_OS_X_VERSION_MAX_ALLOWED >= 1090
    if ([[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)])
        [[NSProcessInfo processInfo ] beginActivityWithOptions: NSActivityUserInitiatedAllowingIdleSystemSleep reason:@"SipWitchQt Desktop"];
#endif
}
