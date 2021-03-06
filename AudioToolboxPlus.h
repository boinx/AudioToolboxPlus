//
//  AudioToolboxPlus.h
//  AudioToolboxPlus
//
//  Created by Stefan Fochler on 05.06.18.
//  Copyright © 2018 Boinx Software Ltd. All rights reserved.
//


//! Project version number for AudioToolboxPlus.
//FOUNDATION_EXPORT double AudioToolboxPlusVersionNumber;

//! Project version string for AudioToolboxPlus.
//FOUNDATION_EXPORT const unsigned char AudioToolboxPlusVersionString[];

// In this header, you should import all the public headers of your framework using statements like #import <AudioToolboxPlus/PublicHeader.h>

// Double-quoted include warnings can not be fixed with Double-quoted because it produces a:
// ERROR: Include of non-modular header inside framework module

#import "ATPAudioCompressionSession.h"

#import "ATPAudioConverter.h"
#import "ATPAudioConverter+Properties.h"
#import "ATPAudioConverter+PropertiesFromDictionary.h"

#import "NSError+ATPError.h"

#import "NSValue+ATPAudioValueRange.h"

#import "TPCircularBuffer.h"
#import "TPCircularBuffer+AudioBufferList.h"
