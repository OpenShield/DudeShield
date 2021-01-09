#include "micpermission.h"
//#import <Foundation/NSUserNotification.h>
//#import <Foundation/NSString.h>
#import <AVFoundation/AVCaptureDevice.h>
int MicPermission::check_permission()
{
    AVAuthorizationStatus status = [AVCaptureDevice authorizationStatusForMediaType:AVMediaTypeAudio];
    if(status != AVAuthorizationStatusAuthorized){
        [AVCaptureDevice requestAccessForMediaType:AVMediaTypeAudio completionHandler:^(BOOL granted) {

        }];
    }
    return status;
}
