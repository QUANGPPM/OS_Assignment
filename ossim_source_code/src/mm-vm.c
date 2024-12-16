//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Virtual memory module mm/mm-vm.c
 */

#include "string.h"
#include "mm.h"
#include <stdlib.h>
#include <stdio.h>
#define PAGING_SWPOFF(pte) PAGING_SWP(pte)
#define PAGING_SWPTYP(pte) PAGING_SWP(pte)
/*enlist_vm_freerg_list - add new rg to freerg_list
 *@mm: memory region
 *@rg_elmt: new region
 *
 */
int enlist_vm_freerg_list(struct mm_struct *mm, struct vm_rg_struct *rg_elmt)
{
// struct vm_rg_struct *rg_node = mm->mmap->vm_freerg_list; 
  struct vm_rg_struct *rg_node;
  if(rg_elmt->vmaid == 0)
    rg_node = mm->mmap->vm_freerg_list;
  else if(rg_elmt->vmaid == 1)
    rg_node = mm->mmap->vm_next->vm_freerg_list;
  
#ifdef MM_PAGIMG_HEAP_GODOWN
  if(rg_elmt->vmaid == 0)
    if (rg_elmt->rg_start >= rg_elmt->rg_end)
      return -1; 
  if(rg_elmt->vmaid == 1) // HEAP
    if (rg_elmt->rg_start <= rg_elmt->rg_end){
      return -1; 
    }
    
#else
   if (rg_elmt->rg_start >= rg_elmt->rg_end)
    return -1; 
#endif
  //when enlist a rg continous to another rg, should merge those 2
  // struct vm_rg_struct *rgit = mm->mmap->vm_freerg_list;
  struct vm_rg_struct *rgit;
  if(rg_elmt->vmaid == 0)
    rgit = mm->mmap->vm_freerg_list;
  else if(rg_elmt->vmaid == 1)
    rgit = mm->mmap->vm_next->vm_freerg_list;
  while(rgit!= NULL){
    if(rgit->rg_start != rgit->rg_end){
      if(rgit->rg_start == rg_elmt->rg_end){
        rgit->rg_start = rg_elmt->rg_start;
        return 0;
      }
      else if(rgit->rg_end == rg_elmt->rg_start){
        rgit->rg_end = rg_elmt->rg_end;
        return 0;
      }
    }
    rgit = rgit->rg_next;
  }

  if (rg_node != NULL)
    rg_elmt->rg_next = rg_node;

  /* Enlist the new region */
  // mm->mmap->vm_freerg_list = rg_elmt;
  
  if(rg_elmt->vmaid == 0)
    mm->mmap->vm_freerg_list = rg_elmt;
  else if(rg_elmt->vmaid == 1)
    mm->mmap->vm_next->vm_freerg_list = rg_elmt;  
  return 0;
}

/*get_vma_by_num - get vm area by numID
 *@mm: memory region
 *@vmaid: ID vm area to alloc memory region
 *
 */
// lấy vm_area với id tương ứng
struct vm_area_struct *get_vma_by_num(struct mm_struct *mm, int vmaid)
{
  struct vm_area_struct *pvma= mm->mmap;

  if(mm->mmap == NULL)
    return NULL;

  int vmait = 0;
  
  while (vmait < vmaid)
  {
    if(pvma == NULL)
	  return NULL;

    vmait++;
    pvma = pvma->vm_next;
  }

  return pvma;
}

/*get_symrg_byid - get mem region by region ID
 *@mm: memory region
 *@rgid: region ID act as symbol index of variable
 *
 */
struct vm_rg_struct *get_symrg_byid(struct mm_struct *mm, int rgid)
{
  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return NULL;

  return &mm->symrgtbl[rgid];
}

/*__alloc - allocate a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *@alloc_addr: address of allocated memory region
 *
 */
int __alloc(struct pcb_t *caller
          , int vmaid
          , int rgid          // ID region, dùng để nhận diện trong symbol table
          , int size
          , int *alloc_addr)
{
#ifdef VM_RG_LIST_DEBUG
    printf("====DATA:================\n");
    if(caller->mm->mmap->vm_freerg_list->vmaid == 0){
      print_list_rg((caller->mm->mmap->vm_freerg_list));
    }
    printf("====HEAP:================\n");
    if(caller->mm->mmap->vm_next->vm_freerg_list->vmaid == 1){
      print_list_rg((caller->mm->mmap->vm_next->vm_freerg_list));
    }
    printf("====finish================\n");  
#endif
  /*Allocate at the toproof */
  struct vm_rg_struct rgnode;

  /* TODO: commit the vmaid */
  rgnode.vmaid = vmaid;   // lấy id vm rg

  if (get_free_vmrg_area(caller, vmaid, size, &rgnode) == 0)  //tìm vùng nhớ trống
  {
    caller->mm->symrgtbl[rgid].rg_start = rgnode.rg_start;
    caller->mm->symrgtbl[rgid].rg_end = rgnode.rg_end;

    caller->mm->symrgtbl[rgid].vmaid = rgnode.vmaid;

    *alloc_addr = rgnode.rg_start;


#ifdef VM_RG_DEBUG
    printf("---get free region:---\n");
    print_list_rg(&(caller->mm->symrgtbl[rgid]));
#endif
    return 0;
  }
//---------------------------------------------------------//

  
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); // lấy vm_area với id tương ứng
#ifdef MM_PAGIMG_HEAP_GODOWN
  int inc_sz, inc_limit_ret, old_sbrk, new_sbrk;
  if(vmaid == 1){   // điều chỉnh dấu 1 chút
      inc_sz = PAGING_PAGE_ALIGNSZ(cur_vma->vm_end - ( cur_vma->sbrk - size)); // giảm bớt việc cấp phát dư thừa

      /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
      old_sbrk = cur_vma->sbrk ;      // mở rộng vùng nhớ từ con trỏ sbrk
      new_sbrk = old_sbrk - size;
      // nếu mở rộng bằng con trỏ sbrk mà không vượt quá giới hạn area -> cấp phát
      if(new_sbrk > cur_vma->vm_end){
        caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
        caller->mm->symrgtbl[rgid].rg_end = new_sbrk;
        caller->mm->symrgtbl[rgid].vmaid = vmaid;
        cur_vma->sbrk = new_sbrk;
        #ifdef VM_RG_DEBUG
        printf("---upate sbrk:---\n");
        print_list_rg(&(caller->mm->symrgtbl[rgid]));
    #endif
        return 0;
      }     
  }
  else if(vmaid == 0){
      inc_sz = PAGING_PAGE_ALIGNSZ((size + cur_vma->sbrk) - cur_vma->vm_end); // giảm bớt việc cấp phát dư thừa

      /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
      old_sbrk = cur_vma->sbrk ;      // mở rộng vùng nhớ từ con trỏ sbrk
      new_sbrk = old_sbrk + size;
      // nếu mở rộng bằng con trỏ sbrk mà không vượt quá giới hạn area -> cấp phát
      if(new_sbrk < cur_vma->vm_end){
        caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
        caller->mm->symrgtbl[rgid].rg_end = new_sbrk;
        caller->mm->symrgtbl[rgid].vmaid = vmaid;
        cur_vma->sbrk = new_sbrk;
        #ifdef VM_RG_DEBUG
        printf("---upate sbrk:---\n");
        print_list_rg(&(caller->mm->symrgtbl[rgid]));
    #endif
        return 0;
      }
  }
#else
  // int inc_sz = PAGING_PAGE_ALIGNSZ(size);     // làm tròn size thành số (n * PAGING_PAGESZ) tương ứng
  int inc_sz = PAGING_PAGE_ALIGNSZ((size + cur_vma->sbrk) - cur_vma->vm_end); // giảm bớt việc cấp phát dư thừa
  int inc_limit_ret; // lưu trữ giá trị mở rộng thêm

  /* TODO retrive old_sbrk if needed, current comment out due to compiler redundant warning*/
  int old_sbrk = cur_vma->sbrk ;      // mở rộng vùng nhớ từ con trỏ sbrk
  int new_sbrk = old_sbrk + size;
  // nếu mở rộng bằng con trỏ sbrk mà không vượt quá giới hạn area -> cấp phát
  if(new_sbrk < cur_vma->vm_end){
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = new_sbrk;
    caller->mm->symrgtbl[rgid].vmaid = vmaid;
    cur_vma->sbrk = new_sbrk;
    #ifdef VM_RG_DEBUG
    printf("---upate sbrk:---\n");
    print_list_rg(&(caller->mm->symrgtbl[rgid]));
#endif
    return 0;
  }
#endif


  /* TODO INCREASE THE LIMIT
   * inc_vma_limit(caller, vmaid, inc_sz)
   */

  /* TODO: commit the limit increment */
  /* TODO: commit the allocation address 
  // *alloc_addr = ...
  */
 #ifdef VM_RG_DEBUG
    printf("---increase limit:---\n");
#endif
 if(inc_vma_limit(caller, vmaid, inc_sz, &inc_limit_ret) == 0){// mở rộng giới hạn area thêm
    caller->mm->symrgtbl[rgid].rg_start = old_sbrk;
    caller->mm->symrgtbl[rgid].rg_end = new_sbrk;
    caller->mm->symrgtbl[rgid].vmaid = vmaid;
    // thay đổi sbrk của vma tương ứng
    cur_vma->sbrk = new_sbrk;
    // if(vmaid == 0)
    //   caller->mm->mmap->sbrk = new_sbrk;
    // else if(vmaid == 1)
    //   caller->mm->mmap->vm_next->sbrk = new_sbrk;
    
    *alloc_addr = old_sbrk;
#ifdef VM_RG_DEBUG
    print_list_rg(&(caller->mm->symrgtbl[rgid]));
#endif

    return 0;
 }
  
  return -1;

}

/*__free - remove a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __free(struct pcb_t *caller, int rgid) // rgid: ID của vùng nhớ cần giải phóng
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  // Dummy initialization for avoding compiler dummay warning
  // in incompleted TODO code rgnode will overwrite through implementing
  // the manipulation of rgid later
  // rgnode->vmaid = 0;  //dummy initialization
  // rgnode->vmaid = 1;  //dummy initialization

  if(rgid < 0 || rgid > PAGING_MAX_SYMTBL_SZ)
    return -1;

  /* TODO: Manage the collect freed region to freerg_list */

  rgnode->vmaid = caller->mm->symrgtbl[rgid].vmaid; // 30
  rgnode->rg_start = caller->mm->symrgtbl[rgid].rg_start;
  rgnode->rg_end = caller->mm->symrgtbl[rgid].rg_end;

   // Kiểm tra vùng nhớ đã được cấp phát chưa
    if (rgnode->rg_start == -1 || rgnode->rg_end == -1)
        return -1;

    // đặt lại
    caller->mm->symrgtbl[rgid].rg_start = -1;
    caller->mm->symrgtbl[rgid].rg_end = -1;
    caller->mm->symrgtbl[rgid].vmaid = -1;

  /*enlist the obsoleted memory region */
  
  enlist_vm_freerg_list(caller->mm, rgnode);

#ifdef VM_RG_REMAIN_DEBUG
    printf("======Free area %d================\n", rgnode->vmaid);
    printf("====DATA: free vm_rg \n");
    if(caller->mm->mmap->vm_freerg_list->vmaid == 0){
      print_list_rg((caller->mm->mmap->vm_freerg_list));
    }
    printf("====HEAP: free vm_rg \n");
    if(caller->mm->mmap->vm_next->vm_freerg_list->vmaid == 1){
      print_list_rg((caller->mm->mmap->vm_next->vm_freerg_list));
    }
    printf("====finish============================\n");  
#endif
  return 0;
}

/*pgalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */
int pgalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 0 */
  return __alloc(proc, 0, reg_index, size, &addr);
}

/*pgmalloc - PAGING-based allocate a region memory
 *@proc:  Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify vaiable in symbole table)
 */
int pgmalloc(struct pcb_t *proc, uint32_t size, uint32_t reg_index)
{
  int addr;

  /* By default using vmaid = 1 */
  return __alloc(proc, 1, reg_index, size, &addr);
}

/*pgfree - PAGING-based free a region memory
 *@proc: Process executing the instruction
 *@size: allocated size 
 *@reg_index: memory region ID (used to identify variable in symbole table)
 */

int pgfree_data(struct pcb_t *proc, uint32_t reg_index)
{
   return __free(proc, reg_index);
}

/*pg_getpage - get the page in ram
 *@mm: memory region
 *@pagenum: PGN
 *@framenum: return FPN
 *@caller: caller
 *
 */
int pg_getpage(struct mm_struct *mm, int pgn, int *fpn, struct pcb_t *caller)
{
  uint32_t pte = mm->pgd[pgn];
  
  if (!PAGING_PTE_PAGE_PRESENT(pte))
  { /* Page is not online, make it actively living */
    printf("=====swap to RW=====\n");
    int vicpgn, swpfpn;
    int tgtfpn = PAGING_SWPOFF(pte);
    int tgtswp_type = PAGING_SWPTYP(pte);
    /* TODO: Play with your paging theory here */
    /* Find victim page */
    if(find_victim_page(caller->mm, &vicpgn) < 0)
      return -1; /* No victim page found */
    while(vicpgn == pgn){
      if(find_victim_page(caller->mm, &vicpgn) < 0)
        return -1; /* No victim page found */
    }
    //uint32_t vicpte = caller->mm->pgd[vicpgn]; không dùng

    /* Get free frame in MEMSWP */
    if(MEMPHY_get_freefp(caller->active_mswp, &swpfpn) < 0)
    {
      struct memphy_struct *mswpit = caller->mswp[tgtswp_type];
      if(MEMPHY_get_freefp(mswpit, &swpfpn) < 0)
        return -1; /* No free frame found */
      __swap_cp_page(mswpit, swpfpn, caller->mram, vicpgn);
      caller->active_mswp = mswpit;
    }
    else{
      __swap_cp_page(caller->active_mswp, swpfpn, caller->mram, vicpgn);

      __swap_cp_page(caller->mswp[tgtswp_type], tgtfpn, caller->mram, vicpgn); 
      MEMPHY_put_freefp(caller->mswp[tgtswp_type], tgtfpn);
      pte_set_swap(&caller->mm->pgd[vicpgn], tgtswp_type, swpfpn);
      pte_set_fpn(&caller->mm->pgd[pgn], vicpgn);

      enlist_pgn_node(&caller->mm->fifo_pgn,pgn);
      pte = caller->mm->pgd[pgn];
    }
    *fpn = PAGING_FPN(pte);
    return 0;
  }

  *fpn = PAGING_FPN(pte);

  return 0;
}

/*pg_getval - read value at given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_getval(struct mm_struct *mm, int addr, BYTE *data, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);  // lấy số trang từ addr
  int off = PAGING_OFFST(addr);// lấy offset trong trang -> vị trí cụ thể ở 1 trang
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0)  // lấy frame từ RAM hoặc SWAP
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off; // tính địa chỉ vật lý

  MEMPHY_read(caller->mram,phyaddr, data);

  return 0;
}

/*pg_setval - write value to given offset
 *@mm: memory region
 *@addr: virtual address to acess 
 *@value: value
 *
 */
int pg_setval(struct mm_struct *mm, int addr, BYTE value, struct pcb_t *caller)
{
  int pgn = PAGING_PGN(addr);
  int off = PAGING_OFFST(addr);
  int fpn;

  /* Get the page to MEMRAM, swap from MEMSWAP if needed */
  if(pg_getpage(mm, pgn, &fpn, caller) != 0) 
    return -1; /* invalid page access */

  int phyaddr = (fpn << PAGING_ADDR_FPN_LOBIT) + off;

  MEMPHY_write(caller->mram,phyaddr, value);

   return 0;
}

/*__read - read value in region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __read(struct pcb_t *caller, int rgid, int offset, BYTE *data)
{
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);  // lấy thông tin region từ index rgid của symbol region table
  if(currg->rg_end==currg->rg_start || offset > currg->rg_end){ 
    printf("Segmentation fault\n");
    return -1;
  }
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); // lấy vma tương ứng

  if(currg == NULL || cur_vma == NULL) /* Invalid memory identify */
	  return -1;

  pg_getval(caller->mm, currg->rg_start + offset, data, caller);

  return 0;
}


/*pgwrite - PAGING-based read a region memory */
int pgread(
		struct pcb_t * proc, // Process executing the instruction
		uint32_t source, // Index of source register
		uint32_t offset, // Source address = [source] + [offset]
		uint32_t destination) 
{
  BYTE data;
  int val = __read(proc, source, offset, &data);

  destination = (uint32_t) data;
#ifdef IODUMP
  printf("read region=%d offset=%d value=%d\n", source, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return val;
}

/*__write - write a region memory
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@offset: offset to acess in memory region 
 *@rgid: memory region ID (used to identify variable in symbole table)
 *@size: allocated size 
 *
 */
int __write(struct pcb_t *caller, int rgid, int offset, BYTE value)
{
  
  struct vm_rg_struct *currg = get_symrg_byid(caller->mm, rgid);  // lấy region tại rgid trong table
  // nếu offset vượt quá currg->rg_end thì báo lỗi
  if(currg->rg_end==currg->rg_start || offset > currg->rg_end){ 
    printf("Segmentation fault\n");
    return -1;
  }
  int vmaid = currg->vmaid;

  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); // lấy area
  
  if(currg == NULL || cur_vma == NULL){ /* Invalid memory identify */
    printf("Invalid memory identify\n");
    return -1;
  }
	  

  pg_setval(caller->mm, currg->rg_start + offset, value, caller);

  return 0;
}

/*pgwrite - PAGING-based write a region memory */
int pgwrite(
		struct pcb_t * proc, // Process executing the instruction
		BYTE data, // Data to be wrttien into memory
		uint32_t destination, // Index of destination register
		uint32_t offset)
{
#ifdef IODUMP
  printf("write region=%d offset=%d value=%d\n", destination, offset, data);
#ifdef PAGETBL_DUMP
  print_pgtbl(proc, 0, -1); //print max TBL
#endif
  MEMPHY_dump(proc->mram);
#endif

  return __write(proc, destination, offset, data);
}


/*free_pcb_memphy - collect all memphy of pcb
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 */
int free_pcb_memph(struct pcb_t *caller)
{
  int pagenum, fpn;
  uint32_t pte;


  for(pagenum = 0; pagenum < PAGING_MAX_PGN; pagenum++)
  {
    pte= caller->mm->pgd[pagenum];

    if (!PAGING_PTE_PAGE_PRESENT(pte))
    {
      fpn = PAGING_PTE_FPN(pte);
      MEMPHY_put_freefp(caller->mram, fpn);
    } else {
      fpn = PAGING_PTE_SWP(pte);
      MEMPHY_put_freefp(caller->active_mswp, fpn);    
    }
  }

  return 0;
}

/*get_vm_area_node - get vm area for a number of pages
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@incpgnum: number of page
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
struct vm_rg_struct* get_vm_area_node_at_brk(struct pcb_t *caller, int vmaid, int size, int alignedsz)
{
  struct vm_rg_struct * newrg;
  /* TODO retrive current vma to obtain newrg, current comment out due to compiler redundant warning*/
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid);

  newrg = malloc(sizeof(struct vm_rg_struct));

  /* TODO: update the newrg boundary
  */
#ifdef MM_PAGIMG_HEAP_GODOWN
  if(vmaid == 1){
    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = cur_vma->sbrk - size;
    newrg->rg_next = NULL;
  }else if(vmaid == 0){
    newrg->rg_start = cur_vma->sbrk;
    newrg->rg_end = cur_vma->sbrk + size;
    newrg->rg_next = NULL;
  }
#else
  newrg->rg_start = cur_vma->sbrk;
  newrg->rg_end = cur_vma->sbrk + size;
  newrg->rg_next = NULL;
#endif
  return newrg;
}

/*validate_overlap_vm_area
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@vmastart: vma end
 *@vmaend: vma end
 *
 */
int validate_overlap_vm_area(struct pcb_t *caller, int vmaid, int vmastart, int vmaend)
{
  struct vm_area_struct *vma = caller->mm->mmap;
  /* TODO validate the planned memory area is not overlapped */
  while (vma != NULL)
    {
        /* Bỏ qua VMA không liên quan */
        if (vma->vm_id == vmaid)
        {
            vma = vma->vm_next;
            continue;
        }

        /* Kiểm tra chồng lấn */
        if ((vmastart >= vma->vm_start && vmastart < vma->vm_end) || 
            (vmaend > vma->vm_start && vmaend <= vma->vm_end) ||
            (vmastart <= vma->vm_start && vmaend >= vma->vm_end))
        {
            return -1; // Phát hiện chồng lấn
        }

        vma = vma->vm_next; // Kiểm tra vùng kế tiếp
    }

    return 0;
}

/*inc_vma_limit - increase vm area limits to reserve space for new variable
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@inc_sz: increment size 
 *@inc_limit_ret: increment limit return
 *
 */
// mở rộng giới hạn vùng địa chỉ ảo (Virtual Memory Area - VMA)
int inc_vma_limit(struct pcb_t *caller, int vmaid, int inc_sz, int* inc_limit_ret)
{
  struct vm_rg_struct * newrg = malloc(sizeof(struct vm_rg_struct));
  int inc_amt = PAGING_PAGE_ALIGNSZ(inc_sz);
  int incnumpage =  inc_amt / PAGING_PAGESZ;    // xác định số page thực tế cần thêm
  struct vm_rg_struct *area = get_vm_area_node_at_brk(caller, vmaid, inc_sz, inc_amt); // lấy thêm vùng nhớ mới tại địa chỉ brk dựa trên inc_sz
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); // lấy area tương ứng id

  int old_end = cur_vma->vm_end;

  /*Validate overlap of obtained region */
  if (validate_overlap_vm_area(caller, vmaid, area->rg_start, area->rg_end) < 0){
    printf("Error: Overlap!!\n");
    return -1; /*Overlap and failed allocation */
  }
    

  /* TODO: Obtain the new vm area based on vmaid */
  if (vm_map_ram(caller, area->rg_start, area->rg_end, 
                    old_end, incnumpage , newrg) < 0){
      free(newrg);
      printf("Error: Can't mapping memory!\n");
      return -1; /* Map the memory to MEMRAM */
    }
    
  cur_vma->vm_end = area->rg_end;   // thay đổi giới hạn
  *inc_limit_ret = cur_vma->vm_end - old_end; // ghi lại kích thước mở rộng

  return 0;

}

/*find_victim_page - find victim page
 *@caller: caller
 *@pgn: return page number
 *
 */
// trả về page number được sử dụng lâu nhất ra ngoài làm victim
int find_victim_page(struct mm_struct *mm, int *retpgn) 
{
  struct pgn_t *pg = mm->fifo_pgn;

  /* TODO: Implement the theorical mechanism to find the victim page */
  if(pg == NULL)
    return -1;

  while(pg->pg_next && pg->pg_next->pg_next != NULL)
    pg = pg->pg_next;
  if(pg->pg_next){
    *retpgn = pg->pg_next->pgn;
    free(pg->pg_next);
    pg->pg_next = NULL;
  }
  else{
    *retpgn = pg->pgn;
    free(pg);
    mm->fifo_pgn = NULL;
  }
  return 0;

}

/*get_free_vmrg_area - get a free vm region
 *@caller: caller
 *@vmaid: ID vm area to alloc memory region
 *@size: allocated size 
 *
 */
int get_free_vmrg_area(struct pcb_t *caller, int vmaid, int size, struct vm_rg_struct *newrg)
{
  struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, vmaid); // lấy vm_area với id tương ứng
  struct vm_rg_struct *rgit = cur_vma->vm_freerg_list;                // lấy danh sách vm_rg còn trống   (đang nghi ngờ - rgit->rg_next = ... mới đúng)

  if (rgit == NULL)
    return -1;

  /* Probe unintialized newrg */
  newrg->rg_start = newrg->rg_end = -1;       // biểu thị rằng chưa tìm thấy vùng nhớ phù hợp




  /* Traverse on list of free vm region to find a fit space */
  while (rgit != NULL && rgit->vmaid == vmaid)            // nếu chưa duyệt hết danh sách và vmaid hợp lệ thì tiếp tục duyệt
  {
#ifdef MM_PAGIMG_HEAP_GODOWN
if(vmaid == 1){
     if (rgit->rg_start - size >= rgit->rg_end)      // nếu rgit đủ kích thước
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start - size;
      /* Update left space in chosen region */
      /**
       * vm rg còn dư sau cấp phát (cập nhật lại không gian trống còn lại).
       * vm rg bị dùng hết (loại bỏ vùng trống khỏi danh sách và cập nhật danh sách vùng trống).
       */
      if (rgit->rg_start - size > rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start - size;
      }

//-----------------------------------------------------------//
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      return 0;
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    } 
}
if(vmaid == 0){
     if (rgit->rg_start + size <= rgit->rg_end)      // nếu rgit đủ kích thước
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;
      /* Update left space in chosen region */
      /**
       * vm rg còn dư sau cấp phát (cập nhật lại không gian trống còn lại).
       * vm rg bị dùng hết (loại bỏ vùng trống khỏi danh sách và cập nhật danh sách vùng trống).
       */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }

//-----------------------------------------------------------//
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      return 0;
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    } 
}
#else
    if (rgit->rg_start + size <= rgit->rg_end)      // nếu rgit đủ kích thước
    { /* Current region has enough space */
      newrg->rg_start = rgit->rg_start;
      newrg->rg_end = rgit->rg_start + size;
      /* Update left space in chosen region */
      /**
       * vm rg còn dư sau cấp phát (cập nhật lại không gian trống còn lại).
       * vm rg bị dùng hết (loại bỏ vùng trống khỏi danh sách và cập nhật danh sách vùng trống).
       */
      if (rgit->rg_start + size < rgit->rg_end)
      {
        rgit->rg_start = rgit->rg_start + size;
      }

//-----------------------------------------------------------//
      else
      { /*Use up all space, remove current node */
        /*Clone next rg node */
        struct vm_rg_struct *nextrg = rgit->rg_next;

        /*Cloning */
        if (nextrg != NULL)
        {
          rgit->rg_start = nextrg->rg_start;
          rgit->rg_end = nextrg->rg_end;

          rgit->rg_next = nextrg->rg_next;

          free(nextrg);
        }
        else
        { /*End of free list */
          rgit->rg_start = rgit->rg_end;	//dummy, size 0 region
          rgit->rg_next = NULL;
        }
      }
      return 0;
    }
    else
    {
      rgit = rgit->rg_next;	// Traverse next rg
    }
#endif

  }

 if(newrg->rg_start == -1) // new region not found
   return -1;

 return 0;
}

//#endif
