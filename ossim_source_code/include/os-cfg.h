#ifndef OSCFG_H
#define OSCFG_H

#define MLQ_SCHED 1
#define MAX_PRIO 140

#define MM_PAGING
// #define MM_PAGIMG_HEAP_GODOWN
#define MM_FIXED_MEMSZ

#define VMDBG 1
#define MMDBG 1
#define IODUMP 1
#define PAGETBL_DUMP 1


//Debug:
// #define REMAIN_SLOT 1           // xem số lượng slot còn lại



#define FIXED_POINT 2000     // xác định điểm bắt đầu cho HEAP 
// #define VM_RG_LIST_DEBUG 1   // hiển thị region list trước khi cấp phát
#define VM_RG_REMAIN_DEBUG 1    // hiển thị số region sau khi free
#define VM_RG_DEBUG 1           // hiển thị cấp phát bộ nhớ bằng pp nào
#define VM_AREA_DEBUG 1      // area init
#define FRAME_FREE_LIST 1       // hiển thị list frame trống ban đầu

#endif
