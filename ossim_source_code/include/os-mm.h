#ifndef OSMM_H
#define OSMM_H

#define MM_PAGING
#define PAGING_MAX_MMSWP 4 /* max number of supported swapped space */
#define PAGING_MAX_SYMTBL_SZ 30

typedef char BYTE;
typedef uint32_t addr_t;
//typedef unsigned int uint32_t;

struct pgn_t{
   int pgn;
   struct pgn_t *pg_next; 
};
//===============================================//
/*
 *  Memory region struct
 */
struct vm_rg_struct {
   int vmaid;

   unsigned long rg_start;
   unsigned long rg_end;

   struct vm_rg_struct *rg_next;
};
//===============================================//
/*
 *  Memory area struct
 */
struct vm_area_struct {
   unsigned long vm_id;
   unsigned long vm_start;
   unsigned long vm_end;

   unsigned long sbrk;
/*
 * Derived field
 * unsigned long vm_limit = vm_end - vm_start
 */
   struct mm_struct *vm_mm;
   struct vm_rg_struct *vm_freerg_list;
   struct vm_area_struct *vm_next;
};
//===============================================//
/* 
 * Memory management struct
 */
struct mm_struct {
   uint32_t *pgd;          // page table

   struct vm_area_struct *mmap;     // area list

   /* Currently we support a fixed number of symbol */
   struct vm_rg_struct symrgtbl[PAGING_MAX_SYMTBL_SZ];      // registers

   /* list of free page */
   struct pgn_t *fifo_pgn;
};
//===============================================////===============================================//
//===============================================////===============================================//
/*
 * FRAME/MEM PHY struct
 */

// đại diện cho từng trang nhớ
struct framephy_struct {
   int fpn;                            //Xác định khung này nằm ở vị trí nào trong bộ nhớ vật lý.
   struct framephy_struct *fp_next;    //trỏ đến khung trang tiếp theo trong danh sách liên kết

   /* Resereed for tracking allocated framed */
   struct mm_struct* owner;            //Trỏ đến mm_struct của process sở hữu khung trang này
};


/**
 * đại diện cho toàn bộ bộ nhớ vật lý
 * quản lý các khung trang, bộ nhớ dữ liệu thực tế và trạng thái truy cập
 */ 
struct memphy_struct {
   /* Basic field of data and size */
   BYTE *storage;                      //trỏ tới vùng dữ liệu thực tế của bộ nhớ vật lý.
   int maxsz;                          //Xác định kích thước tối đa của bộ nhớ vật lý (tính theo byte).
   
   /* Sequential device fields */ 
   int rdmflg;                         //Cờ xác định cách truy cập bộ nhớ
   int cursor;                         //Chỉ số hiện tại của con trỏ trong chế độ truy cập tuần tự.

   /* Management structure */
   struct framephy_struct *free_fp_list;
   struct framephy_struct *used_fp_list;
};

#endif
