#ifndef RINGBUFFER_H
#define RINGBUFFER_H

#include <stdint.h>

typedef struct
{

	uint32_t readPointer;
	uint32_t writePointer;
	int32_t size;

	void *buffer[];

} RingBuffer;

RingBuffer *ringBufferInit(int64_t size);
int ringBufferAdd(RingBuffer *buffer, void *value);
void * ringBufferGet(RingBuffer *buffer);
void ringBufferDestroy(RingBuffer *buffer);

#endif
