#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <switch.h>

u32 __nx_applet_type = AppletType_None;

Service hidService;

#define INNER_HEAP_SIZE 0x40000
size_t nx_inner_heap_size = INNER_HEAP_SIZE;
char   nx_inner_heap[INNER_HEAP_SIZE];

void __libnx_initheap(void)
{
    void*  addr = nx_inner_heap;
    size_t size = nx_inner_heap_size;
    extern char* fake_heap_start;
    extern char* fake_heap_end;
    fake_heap_start = (char*)addr;
    fake_heap_end   = (char*)addr + size;
}

void __attribute__((weak)) __appInit(void)
{
    Result rc;

    rc = smInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_SM));

    rc = setsysInitialize();
    if (R_SUCCEEDED(rc))
    {
        SetSysFirmwareVersion fw;
        rc = setsysGetFirmwareVersion(&fw);
        if (R_SUCCEEDED(rc))
            hosversionSet(MAKEHOSVERSION(fw.major, fw.minor, fw.micro));
        setsysExit();
    }

    rc = fsInitialize();
    if (R_FAILED(rc))
        fatalSimple(rc);

    rc = fsdevMountSdmc();
    if (R_FAILED(rc))
        fatalSimple(rc);

    rc = hidInitialize();
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));

    rc = smGetService(&hidService, "hid:sys");
    if (R_FAILED(rc))
        fatalSimple(MAKERESULT(Module_Libnx, LibnxError_InitFail_HID));
}

void __attribute__((weak)) __appExit(void)
{
    serviceClose(&hidService);
    hidExit();
    fsdevUnmountAll();
    fsExit();
    smExit();
}

/// Mini Cycle struct for \ref HidsysNotificationLedPattern.
typedef struct {
    u8 ledIntensity;        ///< Mini Cycle X LED Intensity.
    u8 transitionSteps;     ///< Fading Transition Steps to Mini Cycle X (Uses PWM). Value 0x0: Instant. Each step duration is based on HidsysNotificationLedPattern::baseMiniCycleDuration.
    u8 finalStepDuration;   ///< Final Step Duration Multiplier of Mini Cycle X. Value 0x0: 12.5ms, 0x1 - xF: 1x - 15x. Value is a Multiplier of HidsysNotificationLedPattern::baseMiniCycleDuration.
    u8 pad;
} HidsysNotificationLedPatternCycle;

/// Structure for \ref hidsysSetNotificationLedPattern.
/// See also: https://switchbrew.org/wiki/HID_services#NotificationLedPattern
/// Only the low 4bits of each used byte in this struct is used.
typedef struct {
    u8 baseMiniCycleDuration;                           ///< Mini Cycle Base Duration. Value 0x1-0xF: 12.5ms - 187.5ms. Value 0x0 = 0ms/OFF.
    u8 totalMiniCycles;                                 ///< Number of Mini Cycles + 1. Value 0x0-0xF: 1 - 16 mini cycles.
    u8 totalFullCycles;                                 ///< Number of Full Cycles. Value 0x1-0xF: 1 - 15 full cycles. Value 0x0 is repeat forever, but if baseMiniCycleDuration is set to 0x0, it does the 1st Mini Cycle with a 12.5ms step duration and then the LED stays on with startIntensity.
    u8 startIntensity;                                  ///< LED Start Intensity. Value 0x0=0% - 0xF=100%.

    HidsysNotificationLedPatternCycle miniCycles[16];   ///< Mini Cycles

    u8 unk_x44[0x2];                                    ///< Unknown
    u8 pad_x46[0x2];                                    ///< Padding
} HidsysNotificationLedPattern;

Result hidsysGetUniquePadIds(u64 *UniquePadIds, size_t count, size_t *total_entries) {
    Result rc;

    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;

    ipcAddRecvStatic(&c, UniquePadIds, sizeof(u64)*count, 0);

    raw = serviceIpcPrepareHeader(&hidService, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 703;

    rc = serviceIpcDispatch(&hidService);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        struct {
            u64 magic;
            u64 result;
            u64 total_entries;
        } *resp;

        serviceIpcParse(&hidService, &r, sizeof(*resp));
        resp = r.Raw;

        if (R_SUCCEEDED(rc) && total_entries) *total_entries = resp->total_entries;
    }

    return rc;
}

Result hidsysSetNotificationLedPattern(const HidsysNotificationLedPattern *pattern, u64 UniquePadId) {
    if (hosversionBefore(7,0,0))
        return MAKERESULT(Module_Libnx, LibnxError_IncompatSysVer);

    Result rc;

    IpcCommand c;
    ipcInitialize(&c);

    struct {
        u64 magic;
        u64 cmd_id;
        HidsysNotificationLedPattern pattern;
        u64 UniquePadId;
    } *raw;

    raw = serviceIpcPrepareHeader(&hidService, &c, sizeof(*raw));

    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 830;
    memcpy(&raw->pattern, pattern, sizeof(*pattern));
    raw->UniquePadId = UniquePadId;

    rc = serviceIpcDispatch(&hidService);

    if (R_SUCCEEDED(rc)) {
        IpcParsedCommand r;
        struct {
            u64 magic;
            u64 result;
            u64 total_entries;
        } *resp;

        serviceIpcParse(&hidService, &r, sizeof(*resp));
        resp = r.Raw;
    }

    return rc;
}

Result fsOpenGameCardDetectionEventNotifier(FsEventNotifier* out)
{
    IpcCommand c;
    ipcInitialize(&c);
    
    struct {
        u64 magic;
        u64 cmd_id;
    } *raw;
    
    raw = serviceIpcPrepareHeader(fsGetServiceSession(), &c, sizeof(*raw));
    
    raw->magic = SFCI_MAGIC;
    raw->cmd_id = 501;
    
    Result rc = serviceIpcDispatch(fsGetServiceSession());
    
    if (R_SUCCEEDED(rc))
    {
        IpcParsedCommand r;
        
        struct {
            u64 magic;
            u64 result;
        } *resp;
        
        serviceIpcParse(fsGetServiceSession(), &r, sizeof(*resp));
        resp = r.Raw;
        
        rc = resp->result;
        
        if (R_SUCCEEDED(rc)) serviceCreateSubservice(&out->s, fsGetServiceSession(), &r, 0);
    }
    
    return rc;
}

//--------------------------------------------------------------------------------------//

bool LedOn = false;

void LightOnHomeButton()
{
    size_t total_entries = 0;
    u64 UniquePadIds[0xA];
    HidsysNotificationLedPattern pattern;
    
    memset(&pattern, 0, sizeof(pattern));
    memset(UniquePadIds, 0, sizeof(UniquePadIds));

    pattern.baseMiniCycleDuration = 0xF;
    pattern.totalMiniCycles = 0x1;
    pattern.totalFullCycles = 0x0;
    pattern.startIntensity = 0xF;

    pattern.miniCycles[0].ledIntensity = 0xF;
    pattern.miniCycles[0].transitionSteps = 0x0;
    pattern.miniCycles[0].finalStepDuration = 0x0;

    Result rc = hidsysGetUniquePadIds(UniquePadIds, 0xA, &total_entries);

    if (R_SUCCEEDED(rc))
    {
        for(size_t i = 0; i < total_entries; i++)
        {
            hidsysSetNotificationLedPattern(&pattern, UniquePadIds[i]);
        }
    }

    LedOn = true;
}

void LightOffHomeButton()
{
    size_t total_entries = 0;
    u64 UniquePadIds[0xA];
    HidsysNotificationLedPattern pattern;
    
    memset(&pattern, 0, sizeof(pattern));
    memset(UniquePadIds, 0, sizeof(UniquePadIds));

    Result rc = hidsysGetUniquePadIds(UniquePadIds, 0xA, &total_entries);

    if (R_SUCCEEDED(rc)) {
        for(size_t i = 0; i < total_entries; i++) {
            hidsysSetNotificationLedPattern(&pattern, UniquePadIds[i]);
        }
    }

    LedOn = false;
}

void LightNotifHomeButton()
{
    size_t total_entries = 0;
    u64 UniquePadIds[0xA];
    HidsysNotificationLedPattern pattern;
    
    memset(&pattern, 0, sizeof(pattern));
    memset(UniquePadIds, 0, sizeof(UniquePadIds));

    pattern.baseMiniCycleDuration = 0x1;
    pattern.totalMiniCycles = 0x2;
    pattern.totalFullCycles = 0x4;
    pattern.startIntensity = 0x2;

    pattern.miniCycles[0].ledIntensity = 0xF;
    pattern.miniCycles[0].transitionSteps = 0x4;
    pattern.miniCycles[0].finalStepDuration = 0x0;

    pattern.miniCycles[1].ledIntensity = 0x2;
    pattern.miniCycles[1].transitionSteps = 0x4;
    pattern.miniCycles[1].finalStepDuration = 0x0;

    Result rc = hidsysGetUniquePadIds(UniquePadIds, 0xA, &total_entries);

    if (R_SUCCEEDED(rc))
    {
        for(size_t i = 0; i < total_entries; i++)
        {
            hidsysSetNotificationLedPattern(&pattern, UniquePadIds[i]);
        }
    }

    LedOn = true;
}

void SendNotifications()
{
    // TODO: Decide to support or not the SdCard removing.

    FsEventNotifier fsGameCardEventNotifier;
    //FsEventNotifier fsSdCardEventNotifier;

    Handle fsGameCardEventHandle;
    //Handle fsSdCardEventHandle;

    Event fsGameCardKernelEvent;
    //Event fsSdCardKernelEvent;

    Result rc = fsOpenGameCardDetectionEventNotifier(&fsGameCardEventNotifier);
    if (R_SUCCEEDED(rc))
    {
        rc = fsEventNotifierGetEventHandle(&fsGameCardEventNotifier, &fsGameCardEventHandle);
        if (R_SUCCEEDED(rc))
        {
            eventLoadRemote(&fsGameCardKernelEvent, fsGameCardEventHandle, false);
        }
        else fatalSimple(rc);
    }
    else fatalSimple(rc);

    /*rc = fsOpenSdCardDetectionEventNotifier(&fsSdCardEventNotifier);
    if (R_SUCCEEDED(rc))
    {
        rc = fsEventNotifierGetEventHandle(&fsSdCardEventNotifier, &fsSdCardEventHandle);
        if (R_SUCCEEDED(rc))
        {
            eventLoadRemote(&fsSdCardKernelEvent, fsSdCardEventHandle, false);
        }
        else fatalSimple(rc);
    }
    else fatalSimple(rc);*/

    int index;

    while(true)
    {
        // TODO: Add mpre events!
        rc = waitMulti(&index, -1, waiterForEvent(&fsGameCardKernelEvent), /*waiterForEvent(&fsSdCardKernelEvent)*/);
        if (R_SUCCEEDED(rc))
        {
            if (LedOn)
            {
                LightNotifHomeButton();
                svcSleepThread(600000000UL);
                LightOnHomeButton();
                eventClear(&fsGameCardKernelEvent);
            }
        }
        else fatalSimple(rc);

        svcSleepThread(10000000UL);
    }
}

int main(int argc, char* argv[])
{
    Thread thread;

    Result rc = threadCreate(&thread, SendNotifications, NULL, 0x10000, 0x2C, -2);
    if (R_SUCCEEDED(rc))
    {
        threadStart(&thread);
    }

    // TODO: Find a better way to trigger On/Off.
    while (true)
    {
        hidScanInput();
        int keys = hidKeysDown(CONTROLLER_P1_AUTO) | hidKeysHeld(CONTROLLER_P1_AUTO);

        if ((keys & KEY_ZL) && (keys & KEY_LSTICK))
        {
            LightOnHomeButton();
        }

        if ((keys & KEY_ZR) && (keys & KEY_LSTICK))
        {
            LightOffHomeButton();
        }

        svcSleepThread(10000000UL);
    }

    return 0;
}
