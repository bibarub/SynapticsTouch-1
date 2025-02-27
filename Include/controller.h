/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		controller.h

	Abstract:

		This module contains touch driver public definitions.

	Environment:

		Kernel mode

	Revision History:

--*/

#pragma once

#include <wdm.h>
#include <wdf.h>
#include <hidport.h>
#define RESHUB_USE_HELPER_ROUTINES
#include <reshub.h>
#include <kbdmou.h>
#include <gpio.h>
#include "spb.h"
#include "hid.h"

#define TOUCH_POOL_TAG                  (ULONG)'cuoT'

//
// Device descriptions
//

#define OEM_MAX_TOUCHES    10

//
// Constants
//
#define MODE_MOUSE                      0x00
#define MODE_SINGLE_TOUCH               0x01
#define MODE_MULTI_TOUCH                0x02

#define MAX_MOUSE_COORD                 0x7FFF
#define MAX_TOUCH_COORD                 0x0FFF

#define MOUSE_LEFT_BUTTON_DOWN          0x0001
#define MOUSE_LEFT_BUTTON_UP            0x0002
#define MOUSE_MOVE_TOUCH_ABSOLUTE       0x8000

#define FINGER_STATUS                   0x01 // finger down

#define REPORTID_MTOUCH                 1
#define REPORTID_MOUSE                  3
#define REPORTID_FEATURE                7
#define REPORTID_MAX_COUNT              8

#define  MAX_REPORTS_IN_QUEUE            10  //5 for 10 touches + 2 for keyboard + 2 for consumer + 1 reserved

// 
// Type defintions
//

#pragma warning(push)
#pragma warning(disable:4201)  // (nameless struct/union)
#include <pshpack1.h>

typedef struct _HID_CONTACT_POINT
{
	UCHAR  bStatus;
	UCHAR  ContactId;
	USHORT wXData;
	USHORT wYData;
} HID_CONTACT_POINT;

typedef struct _HID_TOUCH_REPORT
{
	union
	{
		struct
		{
			HID_CONTACT_POINT Contacts[SYNAPTICS_TOUCH_DIGITIZER_FINGER_REPORT_COUNT];
			UCHAR  ActualCount;
			USHORT ScanTime;
		} InputReport;
		UCHAR RawInput[15];
	};
} HID_TOUCH_REPORT, * PHID_TOUCH_REPORT;

typedef struct _HID_MOUSE_REPORT {
	union
	{
		struct
		{
			UCHAR  bButtons;
			USHORT wXData;
			USHORT wYData;
			USHORT wReserved;
		}InputReport;
		UCHAR RawInput[7];
	};
} HID_MOUSE_REPORT, * PHID_MOUSE_REPORT;

typedef struct _HID_KEY_REPORT {
	UCHAR  bKeys;
} HID_KEY_REPORT, * PHID_KEY_REPORT;


typedef struct _HID_FEATURE_REPORT
{
	UCHAR ReportID;
	UCHAR InputMode;
	UCHAR DeviceIndex;
} HID_FEATURE_REPORT, * PHID_FEATURE_REPORT;

typedef struct _HID_MAX_COUNT_REPORT
{
	UCHAR ReportID;
	UCHAR MaxCount;
}HID_MAX_COUNT_REPORT, * PHID_MAX_COUNT_REPORT;

typedef struct _HID_INPUT_REPORT
{
	UCHAR ReportID;
	union
	{
		HID_TOUCH_REPORT TouchReport;
		HID_MOUSE_REPORT MouseReport;
		HID_KEY_REPORT   KeyReport;
	};
#ifdef _TIMESTAMP_
	LARGE_INTEGER TimeStamp;
#endif
} HID_INPUT_REPORT, * PHID_INPUT_REPORT;

#include <poppack.h>
#pragma warning(pop)

NTSTATUS
TchAllocateContext(
	OUT VOID** ControllerContext,
	IN WDFDEVICE FxDevice
);

NTSTATUS
TchFreeContext(
	IN VOID* ControllerContext
);

NTSTATUS
TchStartDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
);

NTSTATUS
TchStopDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
);

NTSTATUS
TchStandbyDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
);

NTSTATUS
TchWakeDevice(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext
);

NTSTATUS
TchRegistryGetControllerSettings(
	IN VOID* ControllerContext
);

NTSTATUS
TchServiceInterrupts(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR InputMode,
    IN PHID_INPUT_REPORT* HidReports,
    IN int* HidReportsLength
);

