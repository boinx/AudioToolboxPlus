//
//  AudioToolboxPlus_Tests.m
//  AudioToolboxPlus Tests
//
//  Created by Maximilian Christ on 20/03/15.
//  Copyright (c) 2015 McZonk. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import <XCTest/XCTest.h>
#import <AudioToolbox/AudioToolbox.h>


@interface AudioToolbox_Tests : XCTestCase

@end

@implementation AudioToolbox_Tests

- (void)setUp
{
	[super setUp];
}

- (void)tearDown
{
	[super tearDown];
}

- (void)testFloatToShortPCMConvert
{
	AudioStreamBasicDescription inASBD = { 0 };
	inASBD.mSampleRate = 44100.0;
	inASBD.mFormatID = kAudioFormatLinearPCM;
	inASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	inASBD.mBytesPerPacket = 8;
	inASBD.mFramesPerPacket = 1;
	inASBD.mBytesPerFrame = 8;
	inASBD.mChannelsPerFrame = 2;
	inASBD.mBitsPerChannel = 32;
	
	AudioStreamBasicDescription outASBD = { 0 };
	outASBD.mSampleRate = 44100.0;
	outASBD.mFormatID = kAudioFormatLinearPCM;
	outASBD.mFormatFlags = kAudioFormatFlagIsSignedInteger | kAudioFormatFlagIsPacked;
	outASBD.mBytesPerPacket = 4;
	outASBD.mFramesPerPacket = 1;
	outASBD.mBytesPerFrame = 4;
	outASBD.mChannelsPerFrame = 2;
	outASBD.mBitsPerChannel = 16;
	
	AudioConverterRef audioConverter = NULL;
	XCTAssert(AudioConverterNew(&inASBD, &outASBD, &audioConverter) == noErr);
	
	float inData[4] = { 0.0, 0.0, 1.0, 1.0 };
	short convertedData[4] = { 0, 0, 0, 0 };
	short outData[4] = { 0, 0, SHRT_MAX, SHRT_MAX };
	
	AudioBufferList inBufferList;
	inBufferList.mNumberBuffers = 1;
	inBufferList.mBuffers[0].mData = inData;
	inBufferList.mBuffers[0].mDataByteSize = sizeof(inData);
	inBufferList.mBuffers[0].mNumberChannels = 2;
	
	AudioBufferList outBufferList;
	outBufferList.mNumberBuffers = 1;
	outBufferList.mBuffers[0].mData = convertedData;
	outBufferList.mBuffers[0].mDataByteSize = sizeof(convertedData);
	outBufferList.mBuffers[0].mNumberChannels = 2;

	XCTAssert(AudioConverterConvertComplexBuffer(audioConverter, 2, &inBufferList, &outBufferList) == noErr);
	
	XCTAssert(memcmp(convertedData, outData, sizeof(convertedData)) == 0);
	
	AudioConverterDispose(audioConverter);
}

- (void)testNonInterleavedFloatToInterleavedFloatPCMConvert
{
	AudioStreamBasicDescription inASBD = { 0 };
	inASBD.mSampleRate = 44100.0;
	inASBD.mFormatID = kAudioFormatLinearPCM;
	inASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked | kAudioFormatFlagIsNonInterleaved;
	inASBD.mBytesPerPacket = 4;
	inASBD.mFramesPerPacket = 1;
	inASBD.mBytesPerFrame = 4;
	inASBD.mChannelsPerFrame = 2;
	inASBD.mBitsPerChannel = 32;
	
	AudioStreamBasicDescription outASBD = { 0 };
	outASBD.mSampleRate = 44100.0;
	outASBD.mFormatID = kAudioFormatLinearPCM;
	outASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	outASBD.mBytesPerPacket = 8;
	outASBD.mFramesPerPacket = 1;
	outASBD.mBytesPerFrame = 8;
	outASBD.mChannelsPerFrame = 2;
	outASBD.mBitsPerChannel = 32;
	
	AudioConverterRef audioConverter = NULL;
	XCTAssert(AudioConverterNew(&inASBD, &outASBD, &audioConverter) == noErr);
	
	float inData0[4] = { 0.0, 0.0, 0.0, 0.0 };
	float inData1[4] = { 1.0, 1.0, 1.0, 1.0 };
	float convertedData[8] = {};
	float outData[8] = { 0.0, 1.0, 0.0, 1.0, 0.0, 1.0, 0.0, 1.0 };
	
	AudioBufferList *inBufferList = malloc(sizeof(AudioBufferList) + sizeof(AudioBuffer));
	inBufferList->mNumberBuffers = 2;
	inBufferList->mBuffers[0].mData = inData0;
	inBufferList->mBuffers[0].mDataByteSize = sizeof(inData0);
	inBufferList->mBuffers[0].mNumberChannels = 1;
	inBufferList->mBuffers[1].mData = inData1;
	inBufferList->mBuffers[1].mDataByteSize = sizeof(inData1);
	inBufferList->mBuffers[1].mNumberChannels = 1;
	
	AudioBufferList outBufferList;
	outBufferList.mNumberBuffers = 1;
	outBufferList.mBuffers[0].mData = convertedData;
	outBufferList.mBuffers[0].mDataByteSize = sizeof(convertedData);
	outBufferList.mBuffers[0].mNumberChannels = 2;
	
	XCTAssert(AudioConverterConvertComplexBuffer(audioConverter, 4, inBufferList, &outBufferList) == noErr);
	
	XCTAssert(memcmp(convertedData, outData, sizeof(convertedData)) == 0);
	
	AudioConverterDispose(audioConverter);
	
	free(inBufferList);
}

- (void)testChannelMap
{
	AudioStreamBasicDescription inASBD = { 0 };
	inASBD.mSampleRate = 44100.0;
	inASBD.mFormatID = kAudioFormatLinearPCM;
	inASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	inASBD.mBytesPerPacket = 16;
	inASBD.mFramesPerPacket = 1;
	inASBD.mBytesPerFrame = 16;
	inASBD.mChannelsPerFrame = 4;
	inASBD.mBitsPerChannel = 32;
	
	AudioStreamBasicDescription outASBD = { 0 };
	outASBD.mSampleRate = 44100.0;
	outASBD.mFormatID = kAudioFormatLinearPCM;
	outASBD.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked;
	outASBD.mBytesPerPacket = 8;
	outASBD.mFramesPerPacket = 1;
	outASBD.mBytesPerFrame = 8;
	outASBD.mChannelsPerFrame = 2;
	outASBD.mBitsPerChannel = 32;
	
	AudioConverterRef audioConverter = NULL;
	XCTAssert(AudioConverterNew(&inASBD, &outASBD, &audioConverter) == noErr);
	
	float inData[16] = { 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0, 0.0, 1.0, 2.0, 3.0};
	float convertedData[8] = {};
	float outData[4] = { 1.0, 3.0, 1.0, 3.0 };
	
	SInt32 channelMap[2] = { 1, 3 };
	XCTAssert(AudioConverterSetProperty(audioConverter, kAudioConverterChannelMap, sizeof(channelMap), &channelMap) == noErr);
	
	AudioBufferList inBufferList = { 0 };
	inBufferList.mNumberBuffers = 1;
	inBufferList.mBuffers[0].mData = inData;
	inBufferList.mBuffers[0].mDataByteSize = sizeof(inData);
	inBufferList.mBuffers[0].mNumberChannels = 4;
	
	AudioBufferList outBufferList;
	outBufferList.mNumberBuffers = 1;
	outBufferList.mBuffers[0].mData = convertedData;
	outBufferList.mBuffers[0].mDataByteSize = sizeof(convertedData);
	outBufferList.mBuffers[0].mNumberChannels = 2;
	
	XCTAssert(AudioConverterConvertComplexBuffer(audioConverter, 4, &inBufferList, &outBufferList) == noErr);
	
	XCTAssert(memcmp(convertedData, outData, sizeof(convertedData)) == 0);
	
	AudioConverterDispose(audioConverter);
}

@end
