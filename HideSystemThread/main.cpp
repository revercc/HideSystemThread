#include "import.h"
#include <ntifs.h>
#include <nthalext.h>

PVOID g_NmiCountText = 0;
PVOID g_Nmiaffinity = 0;
PVOID g_Nmipag = 0;

VOID kernel_sleep(LONG msec)
{
    LARGE_INTEGER my_interval;
    my_interval.QuadPart = DELAY_ONE_MILLISECOND;
    my_interval.QuadPart *= msec;
    KeDelayExecutionThread(KernelMode, 0, &my_interval);
}

VOID check_hide_thread()
{
    static int count = 0;
    PKPCR p_current_kpcr;
    p_current_kpcr = GetCurrentKpcr();
    if (!MmIsAddressValid(p_current_kpcr)) {
        return;
    }
    PVOID current_kprcb = p_current_kpcr->CurrentPrcb;
    _KTHREAD* p_kthread = NULL;

    //CurrentThread
    p_kthread = *(_KTHREAD**)((UCHAR*)current_kprcb + CurrentThread);
    if (MmIsAddressValid(p_kthread)) {
        DbgPrint(" irql = %d,count=%d ,number = %d, tid: %p\r\n", KeGetCurrentIrql(), ++count, KeGetCurrentProcessorNumber(), PsGetThreadId(p_kthread));
    }

    //NextThread
    p_kthread = *(_KTHREAD**)((UCHAR*)current_kprcb + NextThread);
    if (MmIsAddressValid(p_kthread)) {
        DbgPrint(" irql = %d,count=%d ,number = %d, tid: %p\r\n", KeGetCurrentIrql(), ++count, KeGetCurrentProcessorNumber(), PsGetThreadId(p_kthread));
    }

    /*
    //enum WaitListHead
    LIST_ENTRY* wait_list_head = (LIST_ENTRY*)((UCHAR*)current_kprcb + WaitListHead);
    LIST_ENTRY* wait_list_entry = wait_list_head->Flink;
    while (wait_list_entry != wait_list_head) {
        p_kthread = (PKTHREAD)((UCHAR*)wait_list_entry - WaitListEntry);
        if (MmIsAddressValid(p_kthread)) {
            DbgPrint(" irql = %d,count=%d ,number = %d, tid: %p\r\n", KeGetCurrentIrql(), ++count, KeGetCurrentProcessorNumber(), PsGetThreadId(p_kthread));
        }
        wait_list_entry = wait_list_entry->Flink;
    }
    */

    //enum DispatcherReadyListHead
    for (int i = 0; i < 32; i++) {
        LIST_ENTRY* ready_list_head = (LIST_ENTRY*)((UCHAR*)current_kprcb + DispatcherReadyListHead) + i;
        LIST_ENTRY* ready_list_entry = ready_list_head->Flink;
        while (ready_list_entry != ready_list_head) {
            p_kthread = (PKTHREAD)((UCHAR*)ready_list_entry - WaitListEntry);
            if (MmIsAddressValid(p_kthread)) {
                DbgPrint(" irql = %d,count=%d ,number = %d, tid: %p\r\n", KeGetCurrentIrql(), ++count, KeGetCurrentProcessorNumber(), PsGetThreadId(p_kthread));
            }
            ready_list_entry = ready_list_entry->Flink;
        }
    }

    //SharedReadyQueue
    for (int i = 0; i < 32; i++) {
        PVOID share_ready_queue = *(PVOID*)((UCHAR*)current_kprcb + SharedReadyQueue);
        LIST_ENTRY* ready_list_head = (LIST_ENTRY*)((UCHAR*)share_ready_queue + 0x10) + i;
        LIST_ENTRY* ready_list_entry = ready_list_head->Flink;
        while (ready_list_entry != ready_list_head) {
            p_kthread = (PKTHREAD)((UCHAR*)ready_list_entry - WaitListEntry);
            if (MmIsAddressValid(p_kthread)) {
                DbgPrint(" irql = %d,count=%d ,number = %d, tid: %p\r\n", KeGetCurrentIrql(), ++count, KeGetCurrentProcessorNumber(), PsGetThreadId(p_kthread));
            }
            ready_list_entry = ready_list_entry->Flink;
        }
    }
}

VOID dpc_call_back(struct _KDPC* Dpc, PVOID DeferredContext, PVOID SystemArgument1, PVOID SystemArgument2)
{ 
    UNREFERENCED_PARAMETER(Dpc);
    UNREFERENCED_PARAMETER(SystemArgument1);
    UNREFERENCED_PARAMETER(DeferredContext);
    UNREFERENCED_PARAMETER(SystemArgument2);
    check_hide_thread();
    if (SystemArgument1 != 0 && MmIsAddressValid(SystemArgument1)) {
        KeSignalCallDpcDone(SystemArgument1);
    }
}

BOOLEAN dpc_thread()
{
    while (1) {
        KeIpiGenericCall((PKIPI_BROADCAST_WORKER)dpc_call_back, NULL);
        kernel_sleep(1000);
    }
}

BOOLEAN nmi_call_back(PVOID Context, BOOLEAN handule)
{
    UNREFERENCED_PARAMETER(Context);
    UNREFERENCED_PARAMETER(handule);
    check_hide_thread();
    //必须返回1不处理0交给操作系统处理（蓝屏）
    return 1;
}

BOOLEAN nmi_thread()
{
    UNICODE_STRING SystemRoutineName = { 0 };
    ULONG cors_num = KeQueryActiveProcessorCountEx(0);
    RtlInitUnicodeString(&SystemRoutineName, L"HalSendNMI");
    _HalSendNMI HalSendNMI = (_HalSendNMI)MmGetSystemRoutineAddress(&SystemRoutineName);
    RtlInitUnicodeString(&SystemRoutineName, L"KeInitializeAffinityEx");
    _KeInitializeAffinityEx KeInitializeAffinityEx = (_KeInitializeAffinityEx)MmGetSystemRoutineAddress(&SystemRoutineName);
    RtlInitUnicodeString(&SystemRoutineName, L"KeAddProcessorAffinityEx");
    _KeAddProcessorAffinityEx KeAddProcessorAffinityEx = (_KeAddProcessorAffinityEx)MmGetSystemRoutineAddress(&SystemRoutineName);
    while (1) {
        //每个核执行一次nmi 2号中断
        for (ULONG i = 0; i < cors_num; i++) {
            KeInitializeAffinityEx((PKAFFINITYEX)g_Nmiaffinity);
            KeAddProcessorAffinityEx((PKAFFINITYEX)g_Nmiaffinity, i);
            HalSendNMI((ULONG64)g_Nmiaffinity);
        }
        kernel_sleep(1000);
    }
}

extern "C" void DriverUnload(PDRIVER_OBJECT driver)
{
    PDEVICE_OBJECT p_device;
    p_device = driver->DeviceObject;
}

extern "C" NTSTATUS DriverEntry(PDRIVER_OBJECT driver, PUNICODE_STRING regPath)
{
    UNREFERENCED_PARAMETER(regPath);
    driver->DriverUnload = DriverUnload;

    //注册NMI回调
    ULONG numCors = KeQueryActiveProcessorCountEx(0);
    g_NmiCountText = ExAllocatePoolWithTag(NonPagedPool, numCors * 0x500, 'tag');
    g_Nmiaffinity = ExAllocatePoolWithTag(NonPagedPool, sizeof(KAFFINITYEX), 'tag');
    g_Nmipag = ExAllocatePoolWithTag(NonPagedPool, 0x1000, 'tag');
    KeRegisterNmiCallback(nmi_call_back, g_NmiCountText);
    
    HANDLE h_Thread = 0;
    OBJECT_ATTRIBUTES attributes = { 0 };
    attributes.Length = sizeof(OBJECT_ATTRIBUTES);
    //创建DPC延迟过程调用线程
    //PsCreateSystemThread(&h_Thread, THREAD_ALL_ACCESS, &attributes, NtCurrentProcess(), NULL, (PKSTART_ROUTINE)dpc_thread, NULL);
    //创建NMI线程
    PsCreateSystemThread(&h_Thread, THREAD_ALL_ACCESS, &attributes, NtCurrentProcess(), NULL, (PKSTART_ROUTINE)nmi_thread, NULL);
    return STATUS_SUCCESS;
}