//#ifdef MM_PAGING
/*
 * PAGING based Memory Management
 * Memory management unit mm/mm.c
 */

#include "mm.h"
#include <stdlib.h>
#include <stdio.h>

/* 
 * init_pte - Initialize PTE entry
 * Khởi tạo một mục trong bảng trang (PTE - Page Table Entry).
 */
int init_pte(uint32_t *pte,
             int pre,    // present: 1 - pre (nghĩa là được ánh xạ vào frame - RAM, SWAP)
             int fpn,    // FPN - Frame Page Number - xác định trong RAM
             int drt,    // dirty - trang có thay đổi hay không
             int swp,    // swap: 1 - nằm trong swap không
             int swptyp, // vùng swap được sử dụng
             int swpoff) //swap offset
{
  if (pre != 0) {
    if (swp == 0) { // Non swap ~ page online
      if (fpn == 0) 
        return -1; // Invalid setting

      /* Valid setting with FPN */
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 
    } else { // page swapped
      SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
      SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);
      CLRBIT(*pte, PAGING_PTE_DIRTY_MASK);

      SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT); 
      SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);
    }
  }

  return 0;   
}

/* 
 * pte_set_swap - Set PTE entry for swapped page
 * @pte    : target page table entry (PTE)
 * @swptyp : swap type
 * @swpoff : swap offset
 * Đánh dấu trang đã được swap ra.
 * dùng khi một trang (page) trong RAM cần được chuyển ra SWAP để giải phóng bộ nhớ RAM
 *    Đặt các cờ PRESENT và SWAPPED.
 *    Lưu thông tin loại swap (swptyp) và vị trí offset (swpoff)
 */
int pte_set_swap(uint32_t *pte, int swptyp, int swpoff)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);    // set present
  SETBIT(*pte, PAGING_PTE_SWAPPED_MASK);    // set swapped

  SETVAL(*pte, swptyp, PAGING_PTE_SWPTYP_MASK, PAGING_PTE_SWPTYP_LOBIT);
  SETVAL(*pte, swpoff, PAGING_PTE_SWPOFF_MASK, PAGING_PTE_SWPOFF_LOBIT);

  return 0;
}

/* 
 * pte_set_swap - Set PTE entry for on-line page
 * @pte   : target page table entry (PTE)
 * @fpn   : frame page number (FPN)
 *  Ánh xạ trang đến khung vật lý trong RAM
 * khi trang cần được tải từ SWAP về lại RAM, hoặc khi trang được cấp phát mới trong RAM
 */
int pte_set_fpn(uint32_t *pte, int fpn)
{
  SETBIT(*pte, PAGING_PTE_PRESENT_MASK);
  CLRBIT(*pte, PAGING_PTE_SWAPPED_MASK);

  SETVAL(*pte, fpn, PAGING_PTE_FPN_MASK, PAGING_PTE_FPN_LOBIT); 

  return 0;
}


/* 
 * vmap_page_range - map a range of page at aligned address
 */
// Ánh xạ một dải trang vào bảng trang của process.
int vmap_page_range(struct pcb_t *caller, // process call
                                int addr, // start address which is aligned to pagesz
                               int pgnum, // num of mapping page
           struct framephy_struct *frames,// list of the mapped frames
              struct vm_rg_struct *ret_rg)// return mapped region, the real mapped fp
{                                         // no guarantee all given pages are mapped
  
  struct framephy_struct *fpit = malloc(sizeof(struct framephy_struct));
  //int  fpn;
  int pgit = 0;
  int pgn = PAGING_PGN(addr); // xác định index page bắt đầu

  /* TODO: update the rg_end and rg_start of ret_rg 
  */
  ret_rg->rg_start = addr;
  ret_rg->rg_end = addr + pgnum*PAGING_PAGESZ;
   

  fpit->fp_next = frames; // trỏ vào danh sách frame ánh xạ

  /* TODO map range of frame to address space 
   * Cấp phát luôn là RAM
   * Thiết lập PTE và ánh xạ vào bộ nhớ RAM
   */
   uint32_t pte = 0; // khởi tạo giá trị cho entry
   for(pgit = 0; pgit < pgnum; pgit++){
    fpit = fpit->fp_next;
    if(fpit == NULL)
        break;
    pte_set_fpn(&pte, fpit->fpn); //thiết lập PTE (cấp phát luôn là RAM)
    caller->mm->pgd[pgn + pgit] = pte;  // page_table[i] = PTE
    enlist_pgn_node(&caller->mm->fifo_pgn, pgn+pgit); // thêm index PTE vào list để quản lý
  }


  return 0;
}

/* 
 * alloc_pages_range - allocate req_pgnum of frame in ram
 * @caller    : caller
 * @req_pgnum : request page num
 * @frm_lst   : frame list
 */
//Cấp phát một số lượng trang từ RAM.
int alloc_pages_range(struct pcb_t *caller              // truy xuất thông tin về RAM
                    , int req_pgnum                     // Số lượng trang cấp phát
                    , struct framephy_struct** frm_lst) // Con trỏ đến danh sách các khung vật lý: Lưu trữ danh sách các khung vật lý được cấp phát thành công
{
  int pgit, fpn;
  struct framephy_struct *newfp_str;
  /* TODO: allocate the page 
  */
  for(pgit = 0; pgit < req_pgnum; pgit++)
  {
    if(MEMPHY_get_freefp(caller->mram, &fpn) == 0)
   {
     newfp_str = (struct framephy_struct *)malloc(sizeof(struct framephy_struct));
     newfp_str->fpn = fpn;
     newfp_str->owner = caller->mm;
     if(*frm_lst == NULL){
      *frm_lst = newfp_str;
     }
     else{
      newfp_str->fp_next = *frm_lst;
      *frm_lst = newfp_str;
     }
   } else {  // ERROR CODE of obtaining somes but not enough frames

      int victim_fpn;     // frame physical number (FP number) 
      int victim_pgn;     // page number (PG number)
      int victim_pte;     // page table entry (PTE)
      int swpfpn = -1;    // frame physical number (FP number) trong vùng swap
      //=====================================================//
      if(find_victim_page(caller->mm, &victim_pgn) < 0){  // trả về index of page để swap ra SWAP
        return -3000; // Out of memory
      }
      victim_pte = caller->mm->pgd[victim_pgn]; // lấy giá trị entry tại index vừa tìm được
      victim_fpn = PAGING_FPN(victim_pte);      // trả về frame cần swap
      // khởi tạo frame mới để cấp phát
      newfp_str = (struct framephy_struct *)malloc (sizeof(struct framephy_struct));
      newfp_str->fpn = victim_fpn; 
      newfp_str->owner = caller->mm;
      //=> tìm đc swap frame + khởi tạo frame trỏ vào swap frame
      //=====================================================//
      // thêm frame mới vào danh sách cấp phát
      if(!*frm_lst){
        *frm_lst = newfp_str; 
      }
      else{
        newfp_str->fp_next = *frm_lst;
        *frm_lst = newfp_str;
      }
      //=====================================================//
      // swap: RAM -> SWAP -> thay đổi PTE
      uint32_t pte = caller->mm->pgd[victim_pgn];
      if(MEMPHY_get_freefp(caller->active_mswp, &swpfpn) == 0){
        printf("swap RAM -> SWAP: PTE-%08x -> ", pte);
        __swap_cp_page(caller->mram, victim_fpn, caller->active_mswp, swpfpn);
        pte_set_swap(&pte, 0, swpfpn);
        caller->mm->pgd[victim_pgn] = pte;
        printf("PTE-%08x\n", pte);
        printf("ramfpn[%d]  ->  swapfpn[%d]\n", victim_fpn, swpfpn);
      }
         
      else 
        return -3000;
   }  
 }

  return 0;
}


/* 
 * vm_map_ram - do the mapping all vm are to ram storage device
 * @caller    : caller
 * @astart    : vm area start
 * @aend      : vm area end
 * @mapstart  : start mapping point
 * @incpgnum  : number of mapped page
 * @ret_rg    : returned region
 */
//Ánh xạ các vùng nhớ từ RAM vào một region của proccess.
int vm_map_ram(struct pcb_t *caller
             , int astart, int aend         // phạm vi virtual mem area cần ánh xạ
             , int mapstart                 // cí trí bắt đầu vùng nhớ ảo cần ánh xạ
             , int incpgnum                 // Số lượng trang cần ánh xạ
             , struct vm_rg_struct *ret_rg) // Con trỏ tới vùng nhớ ảo được ánh xạ (region)
{
  struct framephy_struct *frm_lst = NULL;
  int ret_alloc;

  /*@bksysnet: author provides a feasible solution of getting frames
   *FATAL logic in here, wrong behaviour if we have not enough page
   *i.e. we request 1000 frames meanwhile our RAM has size of 3 frames
   *Don't try to perform that case in this simple work, it will result
   *in endless procedure of swap-off to get frame and we have not provide 
   *duplicate control mechanism, keep it simple
   */
  ret_alloc = alloc_pages_range(caller, incpgnum, &frm_lst); // cap phat so trang

  if (ret_alloc < 0 && ret_alloc != -3000)
    return -1;

  /* Out of memory */
  if (ret_alloc == -3000) 
  {
#ifdef MMDBG
     printf("OOM: vm_map_ram out of memory \n");
#endif
     return -1;
  }

  /* it leaves the case of memory is enough but half in ram, half in swap
   * do the swaping all to swapper to get the all in ram */
  vmap_page_range(caller, mapstart, incpgnum, frm_lst, ret_rg);
  //giải phóng vùng nhớ:
  while(frm_lst != NULL){
    struct framephy_struct *tmp = frm_lst;
    frm_lst = frm_lst->fp_next;
    free(tmp);
  }
  return 0;
}

/* Swap copy content page from source frame to destination frame 
 * @mpsrc  : source memphy
 * @srcfpn : source physical page number (FPN)
 * @mpdst  : destination memphy
 * @dstfpn : destination physical page number (FPN)
 **/
//Swap dữ liệu giữa các trang
int __swap_cp_page(struct memphy_struct *mpsrc, int srcfpn,
                struct memphy_struct *mpdst, int dstfpn) 
{
  int cellidx;
  int addrsrc,addrdst;
  for(cellidx = 0; cellidx < PAGING_PAGESZ; cellidx++)
  {
    addrsrc = srcfpn * PAGING_PAGESZ + cellidx;
    addrdst = dstfpn * PAGING_PAGESZ + cellidx;

    BYTE data;
    MEMPHY_read(mpsrc, addrsrc, &data);
    MEMPHY_write(mpdst, addrdst, data);
  }

  return 0;
}

/*
 *Initialize a empty Memory Management instance
 * @mm:     self mm
 * @caller: mm owner
 */
//Khởi tạo cấu trúc quản lý bộ nhớ (mm_struct) cho một process.
int init_mm(struct mm_struct *mm, struct pcb_t *caller)
{
  struct vm_area_struct * vma0 = malloc(sizeof(struct vm_area_struct)); //data
  struct vm_area_struct * vma1 = malloc(sizeof(struct vm_area_struct)); // heap

  //cấp phát bộ nhớ cho page table
#ifdef MM_PAGIMG_HEAP_GODOWN
  mm->pgd = malloc((caller->vmemsz/PAGING_PAGESZ)*sizeof(uint32_t));
#else
  mm->pgd = malloc(PAGING_MAX_PGN*sizeof(uint32_t));
#endif
  

  /* By default the owner comes with at least one vma for DATA */
  vma0->vm_id = 0;                  //id = 0
  vma0->vm_start = 0;               //
  vma0->vm_end = vma0->vm_start;    //
  vma0->sbrk = vma0->vm_start;
  struct vm_rg_struct *first_rg = init_vm_rg(vma0->vm_start, vma0->vm_end, 0); // khởi tạo region đầu tiên
  enlist_vm_rg_node(&vma0->vm_freerg_list, first_rg);

  /* TODO update VMA0 next */
  vma0->vm_next = vma1; // liên kết với heap

  /* TODO: update one vma for HEAP */
#ifdef MM_PAGIMG_HEAP_GODOWN
  vma1->vm_id = 1;                  //id = 1
  vma1->vm_start = caller->vmemsz;
  vma1->vm_end = vma1->vm_start;
  vma1->sbrk = vma1->vm_start;
#else
  vma1->vm_id = 1;                  //id = 1
  vma1->vm_start = FIXED_POINT;     // giả sử bắt đầu tại 10 000
  vma1->vm_end = vma1->vm_start;
  vma1->sbrk = vma1->vm_start;

#endif
  struct vm_rg_struct *second_rg = init_vm_rg(vma1->vm_start, vma1->vm_end, 1); // khởi tạo region thứ 2
  vma1->vm_next = NULL;
  enlist_vm_rg_node(&vma1->vm_freerg_list, second_rg);

  /* Point vma owner backward */
  vma0->vm_mm = mm; 
  vma1->vm_mm = mm;

  /* TODO: update mmap */
  mm->mmap = vma0;
#ifdef VM_AREA_DEBUG
  printf("======DATA_HEAP_init=====\n");
  print_list_vma(vma0);
#endif
  /* Initialize fifo_pgn */
  mm->fifo_pgn = NULL;
  return 0;
}
//Khởi tạo một vùng nhớ (vm_rg_struct) với thông tin về địa chỉ bắt đầu, kết thúc, và ID.
struct vm_rg_struct* init_vm_rg(int rg_start, int rg_end, int vmaid)
{
  struct vm_rg_struct *rgnode = malloc(sizeof(struct vm_rg_struct));

  rgnode->rg_start = rg_start;
  rgnode->rg_end = rg_end;
  rgnode->vmaid = vmaid;
  rgnode->rg_next = NULL;

  return rgnode;
}
//Thêm một vm_rg_struct vào danh sách liên kết của các vùng nhớ.
int enlist_vm_rg_node(struct vm_rg_struct **rglist, struct vm_rg_struct* rgnode)
{
  rgnode->rg_next = *rglist;
  *rglist = rgnode;

  return 0;
}
//Thêm trang được ánh xạ vào hàng đợi FIFO
int enlist_pgn_node(struct pgn_t **plist, int pgn)
{
  struct pgn_t* pnode = malloc(sizeof(struct pgn_t));

  pnode->pgn = pgn;
  pnode->pg_next = *plist;
  *plist = pnode;

  return 0;
}
// Hiển thị danh sách các khung trang vật lý (frame) trong bộ nhớ.
int print_list_fp(struct framephy_struct *ifp)
{
   struct framephy_struct *fp = ifp;
 
   printf("print_list_fp: ");
   if (fp == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   printf("fp[%d]",fp->fpn);
   printf("...");
   while (fp->fp_next != NULL )
   {
      fp = fp->fp_next;
      //  printf("fp[%d]\n",fp->fpn);
       if(fp->fp_next == NULL){
          printf("fp[%d]",fp->fpn);
          break;
       }
   }

   printf("\n==================\n");
   return 0;
}
// Hiển thị danh sách các vùng nhớ (region) trống hoặc đã sử dụng trong bộ nhớ ảo của một tiến trình.
int print_list_rg(struct vm_rg_struct *irg)
{
   struct vm_rg_struct *rg = irg;
 
  //  printf("print_list_rg: ");
   if (rg == NULL) {printf("NULL list\n"); return -1;}
  //  printf("\n");
   while (rg != NULL)
   {
       printf("rg[%ld->%ld<at>vma=%d]\n",rg->rg_start, rg->rg_end, rg->vmaid);
       rg = rg->rg_next;
   }
   printf("\n");
   return 0;
}
// Hiển thị thông tin các vùng địa chỉ ảo (virtual memory area - VMA) được ánh xạ trong không gian địa chỉ của một tiến trình.
int print_list_vma(struct vm_area_struct *ivma)
{
   struct vm_area_struct *vma = ivma;
 
   printf("print_list_vma: ");
   if (vma == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (vma != NULL )
   {
       printf("va[%ld->%ld]\n",vma->vm_start, vma->vm_end);
       vma = vma->vm_next;
   }
   printf("\n");
   return 0;
}
// Hiển thị danh sách các trang (page) được sử dụng hoặc hoán đổi (swapped) trong bộ nhớ ảo.
int print_list_pgn(struct pgn_t *ip)
{
   printf("print_list_pgn: ");
   if (ip == NULL) {printf("NULL list\n"); return -1;}
   printf("\n");
   while (ip != NULL )
   {
       printf("va[%d]-\n",ip->pgn);
       ip = ip->pg_next;
   }
   printf("n");
   return 0;
}
// Hiển thị bảng trang (page table) của tiến trình, bao gồm thông tin ánh xạ từ địa chỉ ảo sang địa chỉ vật lý hoặc vùng swap.
int print_pgtbl(struct pcb_t *caller, uint32_t start, uint32_t end)
{
  int pgn_start,pgn_end;
  int pgit;

  if(end == -1){    // không có địa chỉ kết thúc cụ thể
    pgn_start = 0;
    struct vm_area_struct *cur_vma = get_vma_by_num(caller->mm, 0); // lấy info vma0
    end = cur_vma->vm_end;
  }
  pgn_start = PAGING_PGN(start);  //chuyển start addr -> số trang 
  pgn_end = PAGING_PGN(end);      //chuyển end addr -> số trang

  // printf(" pgn: %d %d\n",pgn_start, pgn_end);

  printf("print_pgtbl: %d - %d", start, end);
  printf("\n");
  if (caller == NULL) {printf("NULL caller\n"); return -1;}
  
  for(pgit = pgn_start; pgit < pgn_end; pgit++)
  {
     printf("%08ld: %08x\n", pgit * sizeof(uint32_t), caller->mm->pgd[pgit]);
  }

  return 0;
}

//#endif
