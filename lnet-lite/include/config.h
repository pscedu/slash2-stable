/* this kernel contains Red Hat 2.4.20 patches */
#define CONFIG_RH_2_4_20 1

/* Define to 1 if you have the <asm/page.h> header file. */
/* #undef HAVE_ASM_PAGE_H */

/* Define to 1 if you have the <asm/types.h> header file. */
#define HAVE_ASM_TYPES_H 1

/* panic_notifier_list is atomic_notifier_head */
/* #undef HAVE_ATOMIC_PANIC_NOTIFIER */

/* Define to 1 if you have the <catamount/data.h> header file. */
/* #undef HAVE_CATAMOUNT_DATA_H */

/* cpumask_t found */
#define HAVE_CPUMASK_T 1

/* cpu_online found */
#define HAVE_CPU_ONLINE 1

/* kmem_cache_destroy(cachep) return int */
#define HAVE_KMEM_CACHE_DESTROY_INT 1

/* readline library is available */
#define HAVE_LIBREADLINE 1

/* Define to 1 if you have the <linux/version.h> header file. */
#define HAVE_LINUX_VERSION_H 1

/* mm_inline found */
/* #undef HAVE_MM_INLINE */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* struct page has a list field */
/* #undef HAVE_PAGE_LIST */

/* Enable quota support */
#define HAVE_QUOTA_SUPPORT 1

/* show_task is exported */
#define HAVE_SHOW_TASK 1

/* Define to 1 if you have the <sys/user.h> header file. */
#define HAVE_SYS_USER_H 1

/* struct file_operations has an unlock ed_ioctl field */
#define HAVE_UNLOCKED_IOCTL 1

/* Max LNET payload */
#define LNET_MAX_PAYLOAD LNET_MTU

#ifndef __LP64__
#define HAVE_U64_LONG_LONG
#endif
