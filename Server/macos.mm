#include <Foundation/NSProcessInfo.h>
#include <Foundation/Foundation.h>

void disable_nap()
{
#if defined(MAC_OS_X_VERSION_MAX_ALLOWED) &&  MAC_OS_X_VERSION_MAX_ALLOWED >= 1090
    if ([[NSProcessInfo processInfo] respondsToSelector:@selector(beginActivityWithOptions:reason:)])
        [[NSProcessInfo processInfo ] beginActivityWithOptions: NSActivityUserInitiatedAllowingIdleSystemSleep reason:@"SipWitchQt Server"];
#endif
}
