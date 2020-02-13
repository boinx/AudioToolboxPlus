//
//  AudioToolboxPlus_iOS.h
//  AudioToolboxPlus-iOS
//
//  Created by Mladen Mikic on 11/02/2020.
//  Copyright © 2020 boinx. All rights reserved.
//

#import <Foundation/Foundation.h>

//! Project version number for AudioToolboxPlus_iOS.
FOUNDATION_EXPORT double AudioToolboxPlus_iOSVersionNumber;

//! Project version string for AudioToolboxPlus_iOS.
FOUNDATION_EXPORT const unsigned char AudioToolboxPlus_iOSVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <AudioToolboxPlus_iOS/PublicHeader.h>

#import "ATPAudioCompressionSession.h"

#import "ATPAudioConverter.h"
#import "ATPAudioConverter+Properties.h"
#import "ATPAudioConverter+PropertiesFromDictionary.h"

#import "NSError+ATPError.h"

#import "NSValue+ATPAudioValueRange.h"

#import "TPCircularBuffer.h"
#import "TPCircularBuffer+AudioBufferList.h"
