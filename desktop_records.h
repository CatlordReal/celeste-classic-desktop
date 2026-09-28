#ifndef DESKTOP_RECORDS_H
#define DESKTOP_RECORDS_H

#include <stddef.h>

#define DESKTOP_RECORD_ANY_PERCENT 0
#define DESKTOP_RECORD_ALL_STRAWBERRIES 1
#define DESKTOP_RECORD_ROOM_COUNT 31

void DesktopRecordInit(void);
int DesktopRecordStartPractice(int category, int room, int allow_web_fallback);
int DesktopRecordWatch(int category);
void DesktopRecordStopPractice(void);

const char* DesktopRecordURL(int category);
const char* DesktopRecordRunner(int category);
const char* DesktopRecordTimeLabel(int category);
const char* DesktopRecordLocalPath(int category);
double DesktopRecordRoomStart(int category, int room);
double DesktopRecordRoomEnd(int category, int room);
int DesktopRecordBuildRoomURL(int category, int room, char* output, size_t output_size);

#endif
