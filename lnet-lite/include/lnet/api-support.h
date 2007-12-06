#ifndef __LNET_API_SUPPORT_H__
#define __LNET_API_SUPPORT_H__

#if defined(__linux__) || defined(__APPLE__)
#include <lnet/linux/api-support.h>
#elif defined(__WINNT__)
#include <lnet/winnt/api-support.h>
#else
#error Unsupported Operating System
#endif

#include <lnet/types.h>
#include <libcfs/kp30.h>
#include <lnet/lnet.h>

#endif
