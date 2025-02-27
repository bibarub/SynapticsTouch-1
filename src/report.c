/*++
	Copyright (c) Microsoft Corporation. All Rights Reserved.
	Sample code. Dealpoint ID #843729.

	Module Name:

		report.c

	Abstract:

		Contains Synaptics specific code for reporting samples

	Environment:

		Kernel mode

	Revision History:

--*/

#include "controller.h"
#include "config.h"
#include "rmiinternal.h"
#include "spb.h"
#include "debug.h"
#include "buttonreporting.h"
#include "hid.h"
#include "Function11.h"
#include "Function12.h"
//#include "report.tmh"

NTSTATUS
RmiGetTouchesFromController(
	IN RMI4_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext
)
{
	NTSTATUS status;

	if (ControllerContext->IsF12Digitizer)
	{
		status = GetTouchesFromF12(ControllerContext, SpbContext);
	}
	else
	{
		status = GetTouchesFromF11(ControllerContext, SpbContext);
	}

	return status;
}

VOID
RmiFillHidReportFromCache(
    IN RMI4_CONTROLLER_CONTEXT* ControllerContext,
	IN PTOUCH_SCREEN_PROPERTIES Props
)
/*++

Routine Description:

	This routine fills a HID report with the next one or two touch entries in
	the local device finger cache.

	The routine also adjusts X/Y coordinates to match the desired display
	coordinates.

Arguments:

	HidReport - pointer to the HID report structure to fill
	Cache - pointer to the local device finger cache
	Props - information on how to adjust X/Y coordinates to match the display
	TouchesReported - On entry, the number of touches (against total) that
		have already been reported. As touches are transferred from the local
		device cache to a HID report, this number is incremented.
	TouchesTotal - total number of touches in the touch cache

Return Value:

	None.

--*/
{
    NTSTATUS status;
    RMI4_FINGER_CACHE* fingerCache = &(ControllerContext->FingerCache);
    RMI4_BUTTONS_CACHE* buttonsCache = &(ControllerContext->ButtonsCache);

	int currentFingerIndex;
	int fingersToReport;
	int i;
    USHORT SctatchX = 0;
    USHORT ScratchY = 0;

    int touchesReported = 0;
    int keyTouchesReported = 0;

    //first report keys
    for(i = 0; i < fingerCache->FingerDownCount; i++)
    {
        USHORT X1 = (USHORT)fingerCache->FingerSlot[fingerCache->FingerDownOrder[i]].x;
        USHORT Y1 = (USHORT)fingerCache->FingerSlot[fingerCache->FingerDownOrder[i]].y;

        ULONG ButtonIndex = TchHandleButtonArea(X1, Y1, Props);

        if(ButtonIndex != BUTTON_NONE)
        {
            fingerCache->IsKey[i] = TRUE;
            keyTouchesReported++;
            if(ButtonIndex != BUTTON_UNKNOWN)
                buttonsCache->PhysicalState[ButtonIndex - 1] = fingerCache->FingerSlot[fingerCache->FingerDownOrder[i]].fingerStatus;
        }
    }
    if(keyTouchesReported > 0)
    {
        status = FillButtonsReportFromCache(ControllerContext);
        if(!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_HID,
                "error reporting touch buttons, status: %x",
                status
            );
        }
    }
    
    UCHAR touchesToReport = ((fingerCache->FingerDownCount - keyTouchesReported) & 0xFF);

    //and report touches
    while(touchesToReport>0)
    {
        fingersToReport = min(
            touchesToReport,
            SYNAPTICS_TOUCH_DIGITIZER_FINGER_REPORT_COUNT
        );

        PHID_INPUT_REPORT hidReport;
        status = GetNextHidReport(ControllerContext, &hidReport);
        if(!NT_SUCCESS(status))
        {
            Trace(
                TRACE_LEVEL_ERROR,
                TRACE_FLAG_HID,
                "can't get report queue slot [fillHidReport(touches)], status: %x",
                status
            );
            goto exit;
        }
        hidReport->ReportID = REPORTID_MTOUCH;

        PHID_TOUCH_REPORT hidTouch = &(hidReport->TouchReport);
        //
        // There are only 16-bits for ScanTime, truncate it
        //
        hidTouch->InputReport.ScanTime = fingerCache->ScanTime & 0xFFFF;

        //
        // Report the count
        // We're sending touches using hybrid mode with 2 fingers in our
        // report descriptor. The first report must indicate the
        // total count of touch fingers detected by the digitizer.
        // The remaining reports must indicate 0 for the count.
        // The first report will have the TouchesReported integer set to 0
        // The others will have it set to something else.
        //
        if(touchesReported == 0)
        {
            hidTouch->InputReport.ActualCount = touchesToReport;
        }
        else
        {
            hidTouch->InputReport.ActualCount = 0;
        }

        //
        // Only two fingers supported yet
        //
        for(currentFingerIndex = 0; currentFingerIndex < fingersToReport; currentFingerIndex++)
        {
            //if this touch reported as key ignore it
            if(fingerCache->IsKey[touchesReported])
            {
                touchesReported++;
                currentFingerIndex--;
                continue;
            }

            int currentlyReporting = fingerCache->FingerDownOrder[touchesReported];

            hidTouch->InputReport.Contacts[currentFingerIndex].ContactId = (UCHAR)currentlyReporting;

            SctatchX = (USHORT)fingerCache->FingerSlot[currentlyReporting].x;
            ScratchY = (USHORT)fingerCache->FingerSlot[currentlyReporting].y;

            //
            // Perform per-platform x/y adjustments to controller coordinates
            //
            TchTranslateToDisplayCoordinates(
                &SctatchX,
                &ScratchY,
                Props);

            hidTouch->InputReport.Contacts[currentFingerIndex].wXData = SctatchX;
            hidTouch->InputReport.Contacts[currentFingerIndex].wYData = ScratchY;

            if(fingerCache->FingerSlot[currentlyReporting].fingerStatus)
            {
                hidTouch->InputReport.Contacts[currentFingerIndex].bStatus = FINGER_STATUS;
            }

            touchesReported++;
            touchesToReport--;

#ifdef COORDS_DEBUG
            Trace(
                TRACE_LEVEL_NOISE,
                TRACE_FLAG_REPORTING,
                "ActualCount %d, ContactId %u X %u Y %u Tip %u",
                hidTouch->InputReport.ActualCount,
                hidTouch->InputReport.Contacts[currentFingerIndex].ContactId,
                hidTouch->InputReport.Contacts[currentFingerIndex].wXData,
                hidTouch->InputReport.Contacts[currentFingerIndex].wYData,
                hidTouch->InputReport.Contacts[currentFingerIndex].bStatus
            );
#endif
        }

    }

exit:
    return;
}

NTSTATUS
RmiServiceTouchDataInterrupt(
	IN RMI4_CONTROLLER_CONTEXT* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR InputMode
)
/*++

Routine Description:

	Called when a touch interrupt needs service. Because we fill HID reports
	with two touches at a time, if more than two touches were read from
	hardware, we may need to complete this request from local cached state.

Arguments:

	ControllerContext - Touch controller context
	SpbContext - A pointer to the current SPB context (I2C, etc)
	HidReport- Buffer to fill with a hid report if touch data is available
	InputMode - Specifies mouse, single-touch, or multi-touch reporting modes
	PendingTouches - Notifies caller if there are more touches to report, to
		complete reporting the full state of fingers on the screen

Return Value:

	NTSTATUS indicating whether or not the current hid report buffer was filled

	PendingTouches also indicates whether the caller should expect more than
		one request to be completed to indicate the full state of fingers on
		the screen
--*/
{
	NTSTATUS status;

	status = STATUS_SUCCESS;

	//
	// If no touches are unreported in our cache, read the next set of touches
	// from hardware.
	//
	//if (ControllerContext->TouchesReported == ControllerContext->TouchesTotal)
	//{
		RmiGetTouchesFromController(ControllerContext, SpbContext);

		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_FLAG_SAMPLES,
				"Error. Can't GetTouches from controller - STATUS %x",
				status
			);

			goto exit;
		}

		//
		// Prepare to report touches via HID reports
		//
        for(int i = 0; i < RMI4_MAX_TOUCHES; i++)
            ControllerContext->FingerCache.IsKey[i] = FALSE;

		//
		// If no touches are present return that no data needed to be reported
		//
		if (ControllerContext->FingerCache.FingerDownCount == 0)
		{
			status = STATUS_NO_DATA_DETECTED;
			goto exit;
		}
	//}

	//
	// Single-finger and HID-mouse input modes not implemented
	//
	if (MODE_MULTI_TOUCH != InputMode)
	{
		Trace(
			TRACE_LEVEL_WARNING,
			TRACE_FLAG_SAMPLES,
			"Unable to report touches, only multitouch mode is supported");

		status = STATUS_NOT_IMPLEMENTED;
		goto exit;
	}

	//
	// Fill report with the next (max of two) cached touches
	//
    RmiFillHidReportFromCache(
        ControllerContext,
        &ControllerContext->Props
    );

	//
	// Update the caller if we still have outstanding touches to report
	//

exit:

	return status;
}

NTSTATUS
TchServiceInterrupts(
	IN VOID* ControllerContext,
	IN SPB_CONTEXT* SpbContext,
	IN UCHAR InputMode,
    IN PHID_INPUT_REPORT* HidReports,
    IN int* HidReportsLength
)
/*++

Routine Description:

	This routine is called in response to an interrupt. The driver will
	service chip interrupts, and if data is available to report to HID,
	fill the Request object buffer with a HID report.

Arguments:

	ControllerContext - Touch controller context
	SpbContext - A pointer to the current i2c context
	HidReport - Pointer to a HID_INPUT_REPORT structure to report to the OS
	InputMode - Specifies mouse, single-touch, or multi-touch reporting modes
	ServicingComplete - Notifies caller if there are more reports needed to
		complete servicing interrupts coming from the hardware.

Return Value:

	NTSTATUS indicating whether or not the current HidReport has been filled

	ServicingComplete indicates whether or not a new report buffer is required
		to complete interrupt processing.
--*/
{
	NTSTATUS status = STATUS_NO_DATA_DETECTED;
	RMI4_CONTROLLER_CONTEXT* controller;

	controller = (RMI4_CONTROLLER_CONTEXT*)ControllerContext;

	//
	// Grab a waitlock to ensure the ISR executes serially and is 
	// protected against power state transitions
	//
	WdfWaitLockAcquire(controller->ControllerLock, NULL);

	//
	// Check the interrupt source if no interrupts are pending processing
	//
	if (controller->InterruptStatus == 0)
	{
		status = RmiCheckInterrupts(
			controller,
			SpbContext,
			&controller->InterruptStatus);

		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_FLAG_INTERRUPT,
				"Error servicing interrupts - STATUS:%X",
				status);

			goto exit;
		}
	}

	//
	// Driver only services 0D cap button and 2D touch messages currently
	//
	if (controller->InterruptStatus &
		~(controller->InterruptTouchMask | controller->InterruptCapButtonsMask))
	{
		Trace(
			TRACE_LEVEL_WARNING,
			TRACE_FLAG_INTERRUPT,
			"Ignoring following interrupt flags - STATUS:%X",
			controller->InterruptStatus &
                ~(controller->InterruptTouchMask | 
                controller->InterruptCapButtonsMask));

		//
		// Mask away flags we don't service
		//
		controller->InterruptStatus &=
            (controller->InterruptTouchMask | controller->InterruptCapButtonsMask);
	}

	//
	// RmiServiceXXX routine will change status to STATUS_SUCCESS if there
	// is a HID report to process.
	//
	status = STATUS_UNSUCCESSFUL;

	//
	// Service a capacitive button event if indicated by hardware
	//
	if (controller->InterruptStatus & controller->InterruptCapButtonsMask)
	{

		status = RmiServiceCapacitiveButtonInterrupt(
			ControllerContext,
			SpbContext);

		//
		// mask cap buttons interupts after service
		//
        controller->InterruptStatus &= ~controller->InterruptCapButtonsMask;

        //
        //report if status unsuccess
		//
		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_FLAG_INTERRUPT,
				"Error processing cap button event - STATUS:%X",
				status);
		}
	}

	//
	// Service a touch data event if indicated by hardware 
	//
	if (controller->InterruptStatus & controller->InterruptTouchMask)
	{

		status = RmiServiceTouchDataInterrupt(
			ControllerContext,
			SpbContext,
			InputMode);

		//
		// clear interupt
		//
		controller->InterruptStatus &= ~controller->InterruptTouchMask;

		//
		// report error
		//
		if (!NT_SUCCESS(status))
		{
			Trace(
				TRACE_LEVEL_ERROR,
				TRACE_FLAG_INTERRUPT,
				"Error processing touch event - STATUS:%X",
				status);
		}
	}

	//
	// Add servicing for additional touch interrupts here
	//

exit:
    
    *HidReports = controller->HidQueue;
    *HidReportsLength = controller->HidQueueCount;
    controller->HidQueueCount = 0;

	WdfWaitLockRelease(controller->ControllerLock);

	return status;
}


NTSTATUS
GetNextHidReport(
    IN RMI4_CONTROLLER_CONTEXT* ControllerContext,
    IN PHID_INPUT_REPORT* HidReport
)
{
    if(ControllerContext->HidQueueCount < MAX_REPORTS_IN_QUEUE)
    {
        *HidReport = &(ControllerContext->HidQueue[ControllerContext->HidQueueCount]);
        ControllerContext->HidQueueCount++;
        RtlZeroMemory(*HidReport, sizeof(HID_INPUT_REPORT));
    }
    else
    {
        return STATUS_NO_MEMORY;
    }

    return STATUS_SUCCESS;
}