#import "ATPAudioCompressionSession.h"

#import "ATPAudioConverter.h"
#import "ATPAudioConverter+Properties.h"

#import "TPCircularBuffer.h"


enum {
	/**
	 * This custom error code is used when an AudioConverterFill…Buffer has no sufficent amount of data. This is not an error if there is enough data at a later time.
	 */
	kAudioConverterErr_Underrun = 'urun',
};

@interface ATPAudioCompressionSession ()
{
	TPCircularBuffer circularBuffer;
}

@property (nonatomic, assign) UInt32 inputBytesPerFrame;

@property (nonatomic, assign) AudioStreamBasicDescription outputFormat;
@property (nonatomic, strong) __attribute__((NSObject)) CMAudioFormatDescriptionRef outputFormatDescription;

@property (nonatomic, strong) ATPAudioConverter *converter;
@property (nonatomic, strong) dispatch_queue_t converterQueue;

@property (nonatomic, weak) id<ATPAudioCompressionSessionDelegate> delegate;
@property (nonatomic, strong) dispatch_queue_t delegateQueue;

@property (nonatomic, assign) CMTime presentationTimeStamp;

@property (nonatomic, assign) BOOL finishing;

@end


@implementation ATPAudioCompressionSession

- (instancetype)initWithOutputFormat:(AudioStreamBasicDescription)outputFormat
{
	self = [super init];
	if(self != nil)
	{
		self.outputFormat = outputFormat;
		
		TPCircularBufferInit(&circularBuffer, 128 * 1024);
	}
	return self;
}

- (void)setDelegate:(id<ATPAudioCompressionSessionDelegate>)delegate queue:(dispatch_queue_t)queue
{
	if(queue == nil)
	{
		queue = dispatch_get_main_queue();
	}
	
	self.delegateQueue = queue;
	self.delegate = delegate;
}

- (void)setupConverterWithFormatDescription:(CMFormatDescriptionRef const)formatDescription
{
	const AudioStreamBasicDescription * const inputFormat = CMAudioFormatDescriptionGetStreamBasicDescription(formatDescription);
	if(inputFormat == nil)
	{
		NSLog(@"%s:%d:TODO: error", __FUNCTION__, __LINE__);
		return;
	}
	
	self.inputBytesPerFrame = inputFormat->mBytesPerFrame;
	
	AudioStreamBasicDescription outputFormat = self.outputFormat;
	
	if(outputFormat.mSampleRate != 0 && outputFormat.mSampleRate != inputFormat->mSampleRate)
	{
		NSLog(@"%s:%d:TODO: error", __FUNCTION__, __LINE__);
		return;
	}
	
	NSError *error = nil;
	ATPAudioConverter * const converter = [[ATPAudioConverter alloc] initWithInputFormat:inputFormat outputFormat:&outputFormat error:&error];
	if (converter == nil)
	{
		NSLog(@"%s:%d:TODO: error %@", __FUNCTION__, __LINE__, error);
		return;
	}
	
	self.converter = converter;
	
	outputFormat = converter.outputFormat;
	self.outputFormat = outputFormat; // some fields will be filled after creating the code
	
	AudioChannelLayout channelLayout = { 0 };
	channelLayout.mChannelLayoutTag = kAudioChannelLayoutTag_Stereo;
	
	NSData *magicCookie = converter.magicCookie;
	
	CMAudioFormatDescriptionRef outputFormatDescription = NULL;
	OSStatus status = CMAudioFormatDescriptionCreate(NULL, &outputFormat, sizeof(channelLayout), &channelLayout, magicCookie.length, magicCookie.bytes, NULL, &outputFormatDescription);
	if (status != noErr)
	{
		NSLog(@"%s:%d:TODO: status %d", __FUNCTION__, __LINE__, status);
		return;
	}
	
	self.outputFormatDescription = outputFormatDescription;
	
	self.presentationTimeStamp = kCMTimeZero;
	
	CFRelease(outputFormatDescription);
}

- (TPCircularBuffer *)circularBuffer
{
	return &circularBuffer;
}

static OSStatus ATPAudioCallback(AudioConverterRef inAudioConverter, UInt32 *ioNumberDataPackets, AudioBufferList *bufferList, AudioStreamPacketDescription **outDataPacketDescription, void *inUserData)
{
	ATPAudioCompressionSession *self = (__bridge ATPAudioCompressionSession *)inUserData;
	
	TPCircularBuffer *circularBuffer = self.circularBuffer;
	
	const UInt32 bytesPerFrame = self.inputBytesPerFrame;
	
	UInt32 bufferLength = *ioNumberDataPackets * bytesPerFrame;
	
	int32_t availableBytes;
	void * const data = TPCircularBufferTail(circularBuffer, &availableBytes);
	
	if(self.finishing)
	{
		bufferLength = availableBytes;
		*ioNumberDataPackets = bufferLength / bytesPerFrame;
	}
	else if(availableBytes < bufferLength)
	{
		return kAudioConverterErr_Underrun;
	}
	
	bufferList->mNumberBuffers = 1;
	
	AudioBuffer * const buffer = &bufferList->mBuffers[0];
	buffer->mNumberChannels = 2;
	buffer->mDataByteSize = bufferLength;
	buffer->mData = data;
	
	TPCircularBufferConsume(circularBuffer, bufferLength);
	
	return noErr;
}

- (void)encodeAudioPacketsWithPresentationTimeStamp:(CMTime)presentationTimeStamp
{
	ATPAudioConverter * const converter = self.converter;
	
	const size_t length = converter.maximumOutputPacketSize;
	
	if (!CMTIME_IS_VALID(presentationTimeStamp)) {
		// Passed in timestamp is invalid, use the backup timestamp instead.
		presentationTimeStamp = self.presentationTimeStamp;
	}
	while(1)
	{
		void *data = malloc(length);
		
		UInt32 outputDataPackets = 1;
		
		AudioBufferList outputData;
		outputData.mNumberBuffers = 1;
		outputData.mBuffers[0].mNumberChannels = 2;
		outputData.mBuffers[0].mDataByteSize = (UInt32)length;
		outputData.mBuffers[0].mData = data;
		
		AudioStreamPacketDescription outputPacketDescription;
		
		OSStatus status = AudioConverterFillComplexBuffer(converter.AudioConverter, ATPAudioCallback, (__bridge void *)self, &outputDataPackets, &outputData, &outputPacketDescription);
		if(status != noErr)
		{
			free(data);
			data = NULL;
			
			if(status == kAudioConverterErr_Underrun)
			{
				break;
			}
			
			break;
		}
		
		CMBlockBufferRef dataBuffer = NULL;
		status = CMBlockBufferCreateWithMemoryBlock(NULL, data, length, NULL, NULL, 0, outputPacketDescription.mDataByteSize, kCMBlockBufferAssureMemoryNowFlag, &dataBuffer); // data is consumed here, no more free
		if(status != noErr)
		{
			break;
		}
		
		CMSampleBufferRef sampleBuffer = NULL;
		CMAudioSampleBufferCreateWithPacketDescriptions(NULL, dataBuffer, true, NULL, NULL, self.outputFormatDescription, 1, presentationTimeStamp, &outputPacketDescription, &sampleBuffer);
		
		presentationTimeStamp = CMTimeAdd(presentationTimeStamp, CMSampleBufferGetDuration(sampleBuffer));
		
		if(sampleBuffer != NULL)
		{
			dispatch_async(self.delegateQueue, ^{
				[self.delegate audioCompressionSession:self didEncodeSampleBuffer:sampleBuffer];
				
				CFRelease(sampleBuffer);
			});
		}
		
		CFRelease(dataBuffer);
	}
	
	self.presentationTimeStamp = presentationTimeStamp;
}

- (BOOL)encodeSampleBuffer:(CMSampleBufferRef const)sampleBuffer
{
	CMFormatDescriptionRef const formatDescription = CMSampleBufferGetFormatDescription(sampleBuffer);
	
	const CMMediaType mediaType = CMFormatDescriptionGetMediaType(formatDescription);
	if(mediaType != kCMMediaType_Audio)
	{
		return NO;
	}
	
	if(self.converter == nil)
	{
		[self setupConverterWithFormatDescription:formatDescription];
	}
		
	// Fill the ring buffer
	{
		CMBlockBufferRef dataBuffer = CMSampleBufferGetDataBuffer(sampleBuffer);
			
		size_t offset = 0;
		size_t length = CMBlockBufferGetDataLength(dataBuffer);

		while(length > 0)
		{
			char *bytes = NULL;
			size_t lengthAtOffset = 0;
			CMBlockBufferGetDataPointer(dataBuffer, offset, &lengthAtOffset, NULL, &bytes);

			if(lengthAtOffset > length)
			{
				lengthAtOffset = length;
			}
			
			if(!TPCircularBufferProduceBytes(&circularBuffer, bytes, (int32_t)lengthAtOffset))
			{
				// the ring buffer is full ...
				break;
			}
			
			offset += lengthAtOffset;
			length -= lengthAtOffset;
		}
	}
	
	CMTime presentationTimeStamp = CMSampleBufferGetPresentationTimeStamp(sampleBuffer);
	[self encodeAudioPacketsWithPresentationTimeStamp:presentationTimeStamp];
	
	return YES;
}

- (BOOL)finish
{
	self.finishing = YES;
	
	[self encodeAudioPacketsWithPresentationTimeStamp:kCMTimeInvalid];
	
	return YES;
}

@end
