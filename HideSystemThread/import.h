#pragma once
#include <ntifs.h>

#define DELAY_ONE_MICROSECOND     (-10)
#define DELAY_ONE_MILLISECOND    (DELAY_ONE_MICROSECOND*1000)

//win10 21H1 19043.928
#define CurrentThread   0x8
#define NextThread      0x10
#define IdleThread      0x18
#define WaitListHead    0x7c00          //[Type:_LIST_ENTRY]
#define DispatcherReadyListHead 0x7c80  //[Type:_LIST_ENTRY[32]]
#define SharedReadyQueue        0x8448  //+ 0x8448 SharedReadyQueue : 0xfffff803`6de1ca00 _KSHARED_READY_QUEUE
#define WaitListEntry   0xd8            //+ 0x0d8 WaitListEntry    : _LIST_ENTRY


typedef struct _KAFFINITY_EX {
    SHORT Count;
    SHORT Size;
    ULONG Padding;
    ULONG64 bitmap[20];
} KAFFINITYEX, * PKAFFINITYEX;

typedef void(__fastcall* _KeInitializeAffinityEx)(PKAFFINITYEX pkaff);
typedef void(__fastcall* _KeAddProcessorAffinityEx)(PKAFFINITYEX pkaff, ULONG nmu);
typedef void(__fastcall* _HalSendNMI)(ULONG64 a1);

extern "C" {
    VOID
        KeGenericCallDpc(
            __in PKDEFERRED_ROUTINE Routine,
            __in_opt PVOID Context
        );

    // Õ∑≈À¯
    VOID
        KeSignalCallDpcDone(
            __in PVOID SystemArgument1
        );

    PKPCR GetCurrentKpcr();
}
