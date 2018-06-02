/*
 * Copyright (C) 2017-2018 Tycho Softworks.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <QtCore/QString>
#include <QtGui/QIcon>
#include <AppKit/NSDockTile.h>
#include <AppKit/NSApplication.h>
#include <AppKit/NSImage.h>
#include <Foundation/NSString.h>
#include <Foundation/NSProcessInfo.h>
#include <Foundation/NSUserNotification.h>
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

void send_notification(const UString& title, const UString& body)
{
    NSString *nsTitle = [[[NSString alloc] initWithUTF8String:title.constData()] autorelease];
    NSString *nsBody = [[[NSString alloc] initWithUTF8String:body.constData()] autorelease];
    NSUserNotification *userNotification = [[[NSUserNotification alloc] init] autorelease];
    userNotification.title = nsTitle;
    userNotification.informativeText = nsBody;
    [[NSUserNotificationCenter defaultUserNotificationCenter] deliverNotification:userNotification];
}
