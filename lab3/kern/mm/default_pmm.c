#include <pmm.h>
#include <list.h>
#include <string.h>
#include <default_pmm.h>

/*  In the First Fit algorithm, the allocator keeps a list of free blocks
 * (known as the free list). Once receiving a allocation request for memory,
 * it scans along the list for the first block that is large enough to satisfy
 * the request. If the chosen block is significantly larger than requested, it
 * is usually splitted, and the remainder will be added into the list as
 * another free block.
 *  Please refer to Page 196~198, Section 8.2 of Yan Wei Min's Chinese book
 * "Data Structure -- C programming language".
*/
// LAB2 EXERCISE 1: YOUR CODE
// you should rewrite functions: `default_init`, `default_init_memmap`,
// `default_alloc_pages`, `default_free_pages`.
/*
 * Details of FFMA
 * (1) Preparation:
 *  In order to implement the First-Fit Memory Allocation (FFMA), we should
 * manage the free memory blocks using a list. The struct `free_area_t` is used
 * for the management of free memory blocks.
 *  First, you should get familiar with the struct `list` in list.h. Struct
 * `list` is a simple doubly linked list implementation. You should know how to
 * USE `list_init`, `list_add`(`list_add_after`), `list_add_before`, `list_del`,
 * `list_next`, `list_prev`.
 *  There's a tricky method that is to transform a general `list` struct to a
 * special struct (such as struct `page`), using the following MACROs: `le2page`
 * (in memlayout.h), (and in future labs: `le2vma` (in vmm.h), `le2proc` (in
 * proc.h), etc).
 * (2) `default_init`:
 *  You can reuse the demo `default_init` function to initialize the `free_list`
 * and set `nr_free` to 0. `free_list` is used to record the free memory blocks.
 * `nr_free` is the total number of the free memory blocks.
 * (3) `default_init_memmap`:
 *  CALL GRAPH: `kern_init` --> `pmm_init` --> `page_init` --> `init_memmap` -->
 * `pmm_manager` --> `init_memmap`.
 *  This function is used to initialize a free block (with parameter `addr_base`,
 * `page_number`). In order to initialize a free block, firstly, you should
 * initialize each page (defined in memlayout.h) in this free block. This
 * procedure includes:
 *  - Setting the bit `PG_property` of `p->flags`, which means this page is
 * valid. P.S. In function `pmm_init` (in pmm.c), the bit `PG_reserved` of
 * `p->flags` is already set.
 *  - If this page is free and is not the first page of a free block,
 * `p->property` should be set to 0.
 *  - If this page is free and is the first page of a free block, `p->property`
 * should be set to be the total number of pages in the block.
 *  - `p->ref` should be 0, because now `p` is free and has no reference.
 *  After that, We can use `p->page_link` to link this page into `free_list`.
 * (e.g.: `list_add_before(&free_list, &(p->page_link));` )
 *  Finally, we should update the sum of the free memory blocks: `nr_free += n`.
 * (4) `default_alloc_pages`:
 *  Search for the first free block (block size >= n) in the free list and reszie
 * the block found, returning the address of this block as the address required by
 * `malloc`.
 *  (4.1)
 *      So you should search the free list like this:
 *          list_entry_t le = &free_list;
 *          while((le=list_next(le)) != &free_list) {
 *          ...
 *      (4.1.1)
 *          In the while loop, get the struct `page` and check if `p->property`
 *      (recording the num of free pages in this block) >= n.
 *              struct Page *p = le2page(le, page_link);
 *              if(p->property >= n){ ...
 *      (4.1.2)
 *          If we find this `p`, it means we've found a free block with its size
 *      >= n, whose first `n` pages can be malloced. Some flag bits of this page
 *      should be set as the following: `PG_reserved = 1`, `PG_property = 0`.
 *      Then, unlink the pages from `free_list`.
 *          (4.1.2.1)
 *              If `p->property > n`, we should re-calculate number of the rest
 *          pages of this free block. (e.g.: `le2page(le,page_link))->property
 *          = p->property - n;`)
 *          (4.1.3)
 *              Re-caluclate `nr_free` (number of the the rest of all free block).
 *          (4.1.4)
 *              return `p`.
 *      (4.2)
 *          If we can not find a free block with its size >=n, then return NULL.
 * (5) `default_free_pages`:
 *  re-link the pages into the free list, and may merge small free blocks into
 * the big ones.
 *  (5.1)
 *      According to the base address of the withdrawed blocks, search the free
 *  list for its correct position (with address from low to high), and insert
 *  the pages. (May use `list_next`, `le2page`, `list_add_before`)
 *  (5.2)
 *      Reset the fields of the pages, such as `p->ref` and `p->flags` (PageProperty)
 *  (5.3)
 *      Try to merge blocks at lower or higher addresses. Notice: This should
 *  change some pages' `p->property` correctly.
 */
// free_area_t free_area;
free_area_t free_area;

#define free_list (free_area.free_list)
#define nr_free (free_area.nr_free) // 空闲内存块总数

static void
default_init(void) {
    list_init(&free_list); // 初始化双向链表
    nr_free = 0; // 空闲块总数nr_free为0
}

static void
default_init_memmap(struct Page *base, size_t n) { // 初始化空闲块
    assert(n > 0); // 函数，如果n>0为真，运行；如果n>0为假，则它先打印一条错误消息然后终止程序
    struct Page *p = base; // 指向base页的p
    for (; p != base + n; p ++) {
        assert(PageReserved(p)); // PageReserved()是用来测试该页是否被内核占用的
        p->flags = p->property = 0; // 描述页帧状态的flags和空闲块总数都设置为0
        set_page_ref(p, 0); // 清空引用，page->ref = 0, ref是记录引用页帧的计数器
    }
    base->property = n; // 把初始化中的base页的property设为n，表示空闲数
    nr_free += n; // 设置全局空闲块总数置为n
    SetPageProperty(base); // 设置该页的标志位
    list_add_before(&free_list, &(base->page_link)); // base页的指针集合插入空闲页链表中
}

static struct Page *
default_alloc_pages(size_t n) { // 在空闲块链表中寻找第一个空闲的块，重新设置该空闲块的大小，返回该块的地址，这个地址会被用于malloc即内存分配
    assert(n > 0);
    if (n > nr_free) { // 想获得的block size n大于所有空闲页的总数，直接返回NULL
        return NULL;
    }
    struct Page *page = NULL;
    // 链表的搜索
    list_entry_t *le = &free_list;
    // TODO: optimize (next-fit)
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link); //链表内的地址转换为Page结构指针
        if (p->property >= n) { // 如果该块中空闲页的总数为>=n
            page = p; // 可得块的首页设为p
            break;
        }
    }
    if (page != NULL) { // 找到了可用的块
        if (page->property > n) { // 可用块的尺寸>=n，其中前n个页可以被分配。
            struct Page *p = page + n; // 分配之后的块对应的首页
            p->property = page->property - n; // 可用的页总数-n（分配的尺寸）
            SetPageProperty(p); // 把分配后的页p设为首页
            list_add_after(&(page->page_link), &(p->page_link)); // 把页p加入空闲页链表
        }
        list_del(&(page->page_link)); // 将被分配的页从空闲页链表中删除
        nr_free -= n; // 更新全局空前页的总数，即减去n
        ClearPageProperty(page); // 把前n个被分配的页标记为不可用
        
    }
    return page;
}

static void
default_free_pages(struct Page *base, size_t n) { // 释放size为n的内存块，可能会将较小的空闲块并入较大的块
    assert(n > 0);
    struct Page *p = base;
    for (; p != base + n; p ++) {
        assert(!PageReserved(p) && !PageProperty(p)); // 从base页开始的n个页都被占用且并非空闲块的首页
        p->flags = 0; // p的标志位设为0
        set_page_ref(p, 0); // page->ref = 0；引用也真的计数设为0
    } //先对块内的各页进行标志位的重置
    base->property = n; // base页作为首页的块重新有了n个可用空闲页，即size+n
    SetPageProperty(base); // 释放base页，即将该页状态置为空闲
    list_entry_t *le = list_next(&free_list); // 以链表头的下一个结点作为起始，在空闲页链表中搜索
    while (le != &free_list) { // 顺序搜索
        p = le2page(le, page_link); // 页p是空闲页表中le结点对应的页
        le = list_next(le);
        // TODO: optimize
        if (base + base->property == p) {  // 如果两个空闲块相邻，base在p前
            base->property += p->property; // 把p中空闲页的总数加到base的空闲页总数上
            ClearPageProperty(p); // 清空p的标志位，因为将p合并到base里
            list_del(&(p->page_link)); // 把p对应的链表结点删除
        }
        else if (p + p->property == base) { // 如果两个空闲块相邻，p在base前
            p->property += base->property; // 以下逻辑与上一个情况相同
            ClearPageProperty(base); // 清空base的标志位
            base = p; // 把页p换为base，因为p是while函数里的临时变量，修改base才有作用
            list_del(&(p->page_link));
        }
    }
    nr_free += n; // 释放完成之后，全局空闲页总数+n
    le = list_next(&free_list);
    while (le != &free_list) {
        p = le2page(le, page_link);
        if (base + base->property <= p) {
            assert(base + base->property != p); // 两空闲块不相邻，合并终止
            break;
        }
        le = list_next(le);
    }
    list_add_before(le, &(base->page_link));
}

static size_t
default_nr_free_pages(void) {
    return nr_free;
}

static void
basic_check(void) {
    struct Page *p0, *p1, *p2;
    p0 = p1 = p2 = NULL;
    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(p0 != p1 && p0 != p2 && p1 != p2);
    assert(page_ref(p0) == 0 && page_ref(p1) == 0 && page_ref(p2) == 0);

    assert(page2pa(p0) < npage * PGSIZE);
    assert(page2pa(p1) < npage * PGSIZE);
    assert(page2pa(p2) < npage * PGSIZE);

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    assert(alloc_page() == NULL);

    free_page(p0);
    free_page(p1);
    free_page(p2);
    assert(nr_free == 3);

    assert((p0 = alloc_page()) != NULL);
    assert((p1 = alloc_page()) != NULL);
    assert((p2 = alloc_page()) != NULL);

    assert(alloc_page() == NULL);

    free_page(p0);
    assert(!list_empty(&free_list));

    struct Page *p;
    assert((p = alloc_page()) == p0);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    free_list = free_list_store;
    nr_free = nr_free_store;

    free_page(p);
    free_page(p1);
    free_page(p2);
}

// LAB2: below code is used to check the first fit allocation algorithm (your EXERCISE 1) 
// NOTICE: You SHOULD NOT CHANGE basic_check, default_check functions!
static void
default_check(void) {
    int count = 0, total = 0;
    list_entry_t *le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        assert(PageProperty(p));
        count ++, total += p->property;
    }
    assert(total == nr_free_pages());

    basic_check();

    struct Page *p0 = alloc_pages(5), *p1, *p2;
    assert(p0 != NULL);
    assert(!PageProperty(p0));

    list_entry_t free_list_store = free_list;
    list_init(&free_list);
    assert(list_empty(&free_list));
    assert(alloc_page() == NULL);

    unsigned int nr_free_store = nr_free;
    nr_free = 0;

    free_pages(p0 + 2, 3);
    assert(alloc_pages(4) == NULL);
    assert(PageProperty(p0 + 2) && p0[2].property == 3);
    assert((p1 = alloc_pages(3)) != NULL);
    assert(alloc_page() == NULL);
    assert(p0 + 2 == p1);

    p2 = p0 + 1;
    free_page(p0);
    free_pages(p1, 3);
    assert(PageProperty(p0) && p0->property == 1);
    assert(PageProperty(p1) && p1->property == 3);

    assert((p0 = alloc_page()) == p2 - 1);
    free_page(p0);
    assert((p0 = alloc_pages(2)) == p2 + 1);

    free_pages(p0, 2);
    free_page(p2);

    assert((p0 = alloc_pages(5)) != NULL);
    assert(alloc_page() == NULL);

    assert(nr_free == 0);
    nr_free = nr_free_store;

    free_list = free_list_store;
    free_pages(p0, 5);

    le = &free_list;
    while ((le = list_next(le)) != &free_list) {
        struct Page *p = le2page(le, page_link);
        count --, total -= p->property;
    }
    assert(count == 0);
    assert(total == 0);
}

const struct pmm_manager default_pmm_manager = {
    .name = "default_pmm_manager",
    .init = default_init,
    .init_memmap = default_init_memmap,
    .alloc_pages = default_alloc_pages,
    .free_pages = default_free_pages,
    .nr_free_pages = default_nr_free_pages,
    .check = default_check,
};

