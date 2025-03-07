//
//  TPCircularBuffer+AudioBufferList.c
//  Circular/Ring buffer implementation
//
//  https://github.com/michaeltyson/TPCircularBuffer
//
//  Created by Michael Tyson on 20/03/2012.
//
//  Copyright (C) 2012-2013 A Tasty Pixel
//
//  This software is provided 'as-is', without any express or implied
//  warranty.  In no event will the authors be held liable for any damages
//  arising from the use of this software.
//
//  Permission is granted to anyone to use this software for any purpose,
//  including commercial applications, and to alter it and redistribute it
//  freely, subject to the following restrictions:
//
//  1. The origin of this software must not be misrepresented; you must not
//     claim that you wrote the original software. If you use this software
//     in a product, an acknowledgment in the product documentation would be
//     appreciated but is not required.
//
//  2. Altered source versions must be plainly marked as such, and must not be
//     misrepresented as being the original software.
//
//  3. This notice may not be removed or altered from any source distribution.
//

#include "TPCircularBuffer+AudioBufferList.h"
#import <mach/mach_time.h>

static double __secondsToHostTicks = 0.0;

static inline unsigned long align16byte(unsigned long val) {
    if ( val & (16-1) ) {
        return val + (16 - (val & (16-1)));
    }
    return val;
}

static inline unsigned long min(unsigned long a, unsigned long b) {
    return a > b ? b : a;
}

bool isSpaceAvailable(TPCircularBuffer *buffer, UInt32 numberOfBuffers, UInt32 bytesPerBuffer, const AudioTimeStamp *inTimestamp) {
    uint32_t availableBytes;
    uint32_t tmp = sizeof(TPCircularBufferABLBlockHeader)+((numberOfBuffers-1)*sizeof(AudioBuffer))+(numberOfBuffers*bytesPerBuffer);
    TPCircularBufferABLBlockHeader *block = (TPCircularBufferABLBlockHeader*)TPCircularBufferHead(buffer, &availableBytes);
    bool isSpaceUnAvailable = ( !block || availableBytes < tmp );
    //printf("\navailableBytes = %d, tmp = %d",availableBytes, tmp);
    return !isSpaceUnAvailable;
}

AudioBufferList *TPCircularBufferPrepareEmptyAudioBufferList(TPCircularBuffer *buffer, UInt32 numberOfBuffers, UInt32 bytesPerBuffer, const AudioTimeStamp *inTimestamp) {
    uint32_t availableBytes;
    uint32_t tmp = sizeof(TPCircularBufferABLBlockHeader)+((numberOfBuffers-1)*sizeof(AudioBuffer))+(numberOfBuffers*bytesPerBuffer);
    TPCircularBufferABLBlockHeader *block = (TPCircularBufferABLBlockHeader*)TPCircularBufferHead(buffer, &availableBytes);
    
    if ( !block || availableBytes < tmp ){
        return NULL;
    }
    
    #ifdef DEBUG
    assert(!((unsigned long)block & 0xF) /* Beware unaligned accesses */);
    #endif
    
    if ( inTimestamp ) {
        memcpy(&block->timestamp, inTimestamp, sizeof(AudioTimeStamp));
    } else {
        memset(&block->timestamp, 0, sizeof(AudioTimeStamp));
    }
    
    memset(&block->bufferList, 0, sizeof(AudioBufferList)+((numberOfBuffers-1)*sizeof(AudioBuffer)));
    block->bufferList.mNumberBuffers = numberOfBuffers;
    
    char *dataPtr = (char*)&block->bufferList + sizeof(AudioBufferList)+((numberOfBuffers-1)*sizeof(AudioBuffer));
    for ( UInt32 i=0; i<numberOfBuffers; i++ ) {
        // Find the next 16-byte aligned memory area
        dataPtr = (char*)align16byte((unsigned long)dataPtr);
        
        if ( (UInt32)((dataPtr + bytesPerBuffer) - (char*)block) > availableBytes ) {
            return NULL;
        }
        
        block->bufferList.mBuffers[i].mData = dataPtr;
        block->bufferList.mBuffers[i].mDataByteSize = bytesPerBuffer;
        block->bufferList.mBuffers[i].mNumberChannels = 1;
        
        dataPtr += bytesPerBuffer;
    }
    
    // Make sure whole buffer (including timestamp and length value) is 16-byte aligned in length
    block->totalLength = (UInt32)align16byte((unsigned long)(dataPtr - (char*)block));
    if ( block->totalLength > availableBytes ) {
        return NULL;
    }
    
    return &block->bufferList;
}

AudioBufferList *TPCircularBufferPrepareEmptyAudioBufferListWithAudioFormat(TPCircularBuffer *buffer, const AudioStreamBasicDescription *audioFormat, UInt32 frameCount, const AudioTimeStamp *timestamp) {
    return TPCircularBufferPrepareEmptyAudioBufferList(buffer,
                                                       (audioFormat->mFormatFlags & kAudioFormatFlagIsNonInterleaved) ? audioFormat->mChannelsPerFrame : 1,
                                                       audioFormat->mBytesPerFrame * frameCount,
                                                       timestamp);
}

void TPCircularBufferProduceAudioBufferList(TPCircularBuffer *buffer, const AudioTimeStamp *inTimestamp) {
    uint32_t availableBytes;
    TPCircularBufferABLBlockHeader *block = (TPCircularBufferABLBlockHeader*)TPCircularBufferHead(buffer, &availableBytes);
    
    assert(block);
    
    #ifdef DEBUG
    assert(!((unsigned long)block & 0xF) /* Beware unaligned accesses */);
    #endif
    
    assert(block->bufferList.mBuffers[0].mDataByteSize > 0);
    
    if ( inTimestamp ) {
        memcpy(&block->timestamp, inTimestamp, sizeof(AudioTimeStamp));
    }
    
    UInt32 calculatedLength = (UInt32)(((char*)block->bufferList.mBuffers[block->bufferList.mNumberBuffers-1].mData + block->bufferList.mBuffers[block->bufferList.mNumberBuffers-1].mDataByteSize) - (char*)block);

    // Make sure whole buffer (including timestamp and length value) is 16-byte aligned in length
    calculatedLength = (UInt32)align16byte(calculatedLength);
    
    assert(calculatedLength <= block->totalLength && calculatedLength <= availableBytes);
    
    block->totalLength = calculatedLength;
    
    TPCircularBufferProduce(buffer, block->totalLength);
}

bool isSpaceAvailableIn(TPCircularBuffer *buffer, const AudioBufferList *inBufferList, const AudioTimeStamp *inTimestamp, UInt32 frames, const AudioStreamBasicDescription *audioDescription) {
    if ( frames == 0 ) return true;
    UInt32 byteCount = inBufferList->mBuffers[0].mDataByteSize;
    if ( byteCount == 0 ) return true;
    return isSpaceAvailable(buffer, inBufferList->mNumberBuffers, byteCount, inTimestamp);
}

bool TPCircularBufferCopyAudioBufferList(TPCircularBuffer *buffer, const AudioBufferList *inBufferList, const AudioTimeStamp *inTimestamp, UInt32 frames, const AudioStreamBasicDescription *audioDescription) {
    if ( frames == 0 ) return true;
    
    UInt32 byteCount = inBufferList->mBuffers[0].mDataByteSize;
    if ( frames != kTPCircularBufferCopyAll ) {
        byteCount = frames * audioDescription->mBytesPerFrame;
        assert(byteCount <= inBufferList->mBuffers[0].mDataByteSize);
    }
    
    if ( byteCount == 0 ) return true;
    
    AudioBufferList *bufferList = TPCircularBufferPrepareEmptyAudioBufferList(buffer, inBufferList->mNumberBuffers, byteCount, inTimestamp);
    if ( !bufferList ) return false;
    
    for ( UInt32 i=0; i<bufferList->mNumberBuffers; i++ ) {
        memcpy(bufferList->mBuffers[i].mData, inBufferList->mBuffers[i].mData, byteCount);
    }
    
    TPCircularBufferProduceAudioBufferList(buffer, NULL);
    
    return true;
}

AudioBufferList *TPCircularBufferNextBufferListAfter(TPCircularBuffer *buffer, const AudioBufferList *bufferList, AudioTimeStamp *outTimestamp) {
    uint32_t availableBytes;
    void *tail = TPCircularBufferTail(buffer, &availableBytes);
    void *end = (char*)tail + availableBytes;
    assert((void*)bufferList > (void*)tail && (void*)bufferList < end);
    
    TPCircularBufferABLBlockHeader *originalBlock = (TPCircularBufferABLBlockHeader*)((char*)bufferList - offsetof(TPCircularBufferABLBlockHeader, bufferList));
    
    #ifdef DEBUG
    assert(!((unsigned long)originalBlock & 0xF) /* Beware unaligned accesses */);
    #endif
    
    TPCircularBufferABLBlockHeader *nextBlock = (TPCircularBufferABLBlockHeader*)((char*)originalBlock + originalBlock->totalLength);
    if ( (void*)nextBlock >= end ) return NULL;
    
    #ifdef DEBUG
    assert(!((unsigned long)nextBlock & 0xF) /* Beware unaligned accesses */);
    #endif
    
    if ( outTimestamp ) {
        memcpy(outTimestamp, &nextBlock->timestamp, sizeof(AudioTimeStamp));
    }
    
    return &nextBlock->bufferList;
}

void TPCircularBufferConsumeNextBufferListPartial(TPCircularBuffer *buffer, UInt32 framesToConsume, const AudioStreamBasicDescription *audioFormat) {
    
    uint32_t dontcare;
    TPCircularBufferABLBlockHeader *block = (TPCircularBufferABLBlockHeader*)TPCircularBufferTail(buffer, &dontcare);
    if ( !block ) return;
    
    #ifdef DEBUG
    assert(!((unsigned long)block & 0xF)); // Beware unaligned accesses
    #endif
    
    UInt32 bytesToConsume = (UInt32)min(framesToConsume * audioFormat->mBytesPerFrame, block->bufferList.mBuffers[0].mDataByteSize);
    
    if ( bytesToConsume == block->bufferList.mBuffers[0].mDataByteSize ) {
        TPCircularBufferConsumeNextBufferList(buffer);
        return;
    }
    
    for ( UInt32 i=0; i<block->bufferList.mNumberBuffers; i++ ) {
        assert(bytesToConsume <= block->bufferList.mBuffers[i].mDataByteSize);
        
        block->bufferList.mBuffers[i].mData = (char*)block->bufferList.mBuffers[i].mData + bytesToConsume;
        block->bufferList.mBuffers[i].mDataByteSize -= bytesToConsume;
    }
    
    if ( block->timestamp.mFlags & kAudioTimeStampSampleTimeValid ) {
        block->timestamp.mSampleTime += framesToConsume;
    }
    if ( block->timestamp.mFlags & kAudioTimeStampHostTimeValid ) {
        if ( __secondsToHostTicks == 0.0 ) {
            mach_timebase_info_data_t tinfo;
            mach_timebase_info(&tinfo);
            __secondsToHostTicks = 1.0 / (((double)tinfo.numer / tinfo.denom) * 1.0e-9);
        }

        block->timestamp.mHostTime += ((double)framesToConsume / audioFormat->mSampleRate) * __secondsToHostTicks;
    }
    
    // Reposition block forward, just before the audio data, ensuring 16-byte alignment
    TPCircularBufferABLBlockHeader *newBlock = (TPCircularBufferABLBlockHeader*)(((unsigned long)block + bytesToConsume) & ~0xFul);
    memmove(newBlock, block, sizeof(TPCircularBufferABLBlockHeader) + (block->bufferList.mNumberBuffers-1)*sizeof(AudioBuffer));
    UInt32 bytesFreed = (UInt32)((intptr_t)newBlock - (intptr_t)block);
    newBlock->totalLength -= bytesFreed;
    TPCircularBufferConsume(buffer, bytesFreed);
}

void TPCircularBufferDequeueBufferListFrames(TPCircularBuffer *buffer, UInt32 *ioLengthInFrames, const AudioBufferList *outputBufferList, AudioTimeStamp *outTimestamp, const AudioStreamBasicDescription *audioFormat) {
    bool hasTimestamp = false;
    UInt32 bytesToGo = *ioLengthInFrames * audioFormat->mBytesPerFrame;
    UInt32 bytesCopied = 0;
    while ( bytesToGo > 0 ) {
        AudioBufferList *bufferList = TPCircularBufferNextBufferList(buffer, !hasTimestamp ? outTimestamp : NULL);
        if ( !bufferList ) break;
        
        hasTimestamp = true;
        UInt32 bytesToCopy = (UInt32)min(bytesToGo, bufferList->mBuffers[0].mDataByteSize);
        
        if ( outputBufferList ) {
            for ( UInt32 i=0; i<outputBufferList->mNumberBuffers; i++ ) {
                assert(bytesCopied + bytesToCopy <= outputBufferList->mBuffers[i].mDataByteSize);
                memcpy((char*)outputBufferList->mBuffers[i].mData + bytesCopied, bufferList->mBuffers[i].mData, bytesToCopy);
            }
        }
        
        TPCircularBufferConsumeNextBufferListPartial(buffer, bytesToCopy/audioFormat->mBytesPerFrame, audioFormat);
        
        bytesToGo -= bytesToCopy;
        bytesCopied += bytesToCopy;
    }
    
    *ioLengthInFrames -= bytesToGo / audioFormat->mBytesPerFrame;
}

UInt32 TPCircularBufferPeekContiguousWrapped(TPCircularBuffer *buffer, AudioTimeStamp *outTimestamp, const AudioStreamBasicDescription *audioFormat, UInt32 contiguousToleranceSampleTime, UInt32 wrapPoint) {
    uint32_t availableBytes;
    TPCircularBufferABLBlockHeader *block = (TPCircularBufferABLBlockHeader*)TPCircularBufferTail(buffer, &availableBytes);
    if ( !block ) return 0;
    
    #ifdef DEBUG
    assert(!((unsigned long)block & 0xF) /* Beware unaligned accesses */);
    #endif
    
    if ( outTimestamp ) {
        memcpy(outTimestamp, &block->timestamp, sizeof(AudioTimeStamp));
    }
    
    void *end = (char*)block + availableBytes;
    
    UInt32 byteCount = 0;
    
    while ( 1 ) {
		if(block->bufferList.mNumberBuffers == 0)
		{
			// This condition (block->bufferList.mNumberBuffers == 0) leads to an endless loop,
			// causing movies to stop.
			// If there are no buffers we don't allowed to fetch mBuffers[0] in the next line anyways.
			break;
		}
        byteCount += block->bufferList.mBuffers[0].mDataByteSize;
        TPCircularBufferABLBlockHeader *nextBlock = (TPCircularBufferABLBlockHeader*)((char*)block + block->totalLength);
        if ( (void*)nextBlock >= end ) {
            break;
        }
        
        if ( contiguousToleranceSampleTime != UINT32_MAX ) {
            UInt32 frames = block->bufferList.mBuffers[0].mDataByteSize / audioFormat->mBytesPerFrame;
            Float64 nextTime = block->timestamp.mSampleTime + frames;
            if ( wrapPoint && nextTime > wrapPoint ) nextTime = fmod(nextTime, wrapPoint);
            Float64 diff = fabs(nextBlock->timestamp.mSampleTime - nextTime);
            if ( diff > contiguousToleranceSampleTime && (!wrapPoint || fabs(diff-wrapPoint) > contiguousToleranceSampleTime) ) {
                break;
            }
        }
        
        #ifdef DEBUG
        assert(!((unsigned long)nextBlock & 0xF) /* Beware unaligned accesses */);
        #endif
        
        block = nextBlock;
    }
    
    return byteCount / audioFormat->mBytesPerFrame;
}

UInt32 TPCircularBufferPeek(TPCircularBuffer *buffer, AudioTimeStamp *outTimestamp, const AudioStreamBasicDescription *audioFormat) {
    return TPCircularBufferPeekContiguousWrapped(buffer, outTimestamp, audioFormat, UINT32_MAX, 0);
}

UInt32 TPCircularBufferPeekContiguous(TPCircularBuffer *buffer, AudioTimeStamp *outTimestamp, const AudioStreamBasicDescription *audioFormat, UInt32 contiguousToleranceSampleTime) {
    return TPCircularBufferPeekContiguousWrapped(buffer, outTimestamp, audioFormat, contiguousToleranceSampleTime, 0);
}

UInt32 TPCircularBufferGetAvailableSpace(TPCircularBuffer *buffer, const AudioStreamBasicDescription *audioFormat) {
    // Look at buffer head; make sure there's space for the block metadata
    uint32_t availableBytes;
    TPCircularBufferABLBlockHeader *block = (TPCircularBufferABLBlockHeader*)TPCircularBufferHead(buffer, &availableBytes);
    if ( !block ) return 0;
    
    #ifdef DEBUG
    assert(!((unsigned long)block & 0xF) /* Beware unaligned accesses */);
    #endif
    
    // Now find out how much 16-byte aligned audio we can store in the space available
    UInt32 numberOfBuffers = audioFormat->mFormatFlags & kAudioFormatFlagIsNonInterleaved ? audioFormat->mChannelsPerFrame : 1;
    char * endOfBuffer = (char*)block + availableBytes;
    char * dataPtr = (char*)align16byte((unsigned long)(&block->bufferList + sizeof(AudioBufferList)+((numberOfBuffers-1)*sizeof(AudioBuffer))));
    if ( dataPtr >= endOfBuffer ) return 0;
    UInt32 availableAudioBytes = (UInt32)(endOfBuffer - dataPtr);
    
    UInt32 availableAudioBytesPerBuffer = availableAudioBytes / numberOfBuffers;
    availableAudioBytesPerBuffer -= (availableAudioBytesPerBuffer % (16-1));
    
    return availableAudioBytesPerBuffer > 0 ? availableAudioBytesPerBuffer / audioFormat->mBytesPerFrame : 0;
}
