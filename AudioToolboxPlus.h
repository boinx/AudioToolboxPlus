//
//  AudioToolboxPlus.h
//  AudioToolboxPlus
//
//  Created by Stefan Fochler on 05.06.18.
//  Copyright © 2018 Boinx Software Ltd. All rights reserved.
//

#import <Cocoa/Cocoa.h>

//! Project version number for AudioToolboxPlus.
FOUNDATION_EXPORT double AudioToolboxPlusVersionNumber;

//! Project version string for AudioToolboxPlus.
FOUNDATION_EXPORT const unsigned char AudioToolboxPlusVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <AudioToolboxPlus/PublicHeader.h>

#import "ATPAudioCompressionSession.h"

#import "ATPAudioConverter.h"
#import "ATPAudioConverter+Properties.h"
#import "ATPAudioConverter+PropertiesFromDictionary.h"

#import "NSError+ATPError.h"

#import "NSValue+ATPAudioValueRange.h"

// In Xcode 9.4. this doesn't work. TPCircularBuffer.h can't be found because it's located in a physical folder.
// TODO : Test in Xcode 9.4.1. and beta 10.X
//#import "TPCircularBuffer.h"

