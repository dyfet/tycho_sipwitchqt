#include <QtCore/QString>
#include <QtGui/QIcon>
#include <AppKit/NSDockTile.h>
#include <AppKit/NSApplication.h>
#include <AppKit/NSImage.h>
#include <Foundation/NSString.h>
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


