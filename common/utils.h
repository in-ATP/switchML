#ifndef UTILS_H
#define UTILS_H

#include <sched.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <stdexcept>

#define MAX_AGTR_COUNT 32000
#define AGTR_TO_USE_PER_APPLICATION 2800

#define EACH_HUGEPAGE_SIZE (2048*1024)

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define KB(x) (static_cast<size_t>(x) << 10)
#define KB_(x) (KB(x) - 1)
#define MB(x) (static_cast<size_t>(x) << 20)
#define MB_(x) (MB(x) - 1)

static void memory_barrier() { asm volatile("" ::: "memory"); }
static void lfence() { asm volatile("lfence" ::: "memory"); }
static void sfence() { asm volatile("sfence" ::: "memory"); }
static void mfence() { asm volatile("mfence" ::: "memory"); }
static void clflush(volatile void* p) { asm volatile("clflush (%0)" ::"r"(p)); }
static void cpuid(unsigned int* eax, unsigned int* ebx, unsigned int* ecx,
                  unsigned int* edx) {
  asm volatile("cpuid"
               : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
               : "0"(*eax), "2"(*ecx));
}

inline void bindingCPU(int num) {
  int result;
  cpu_set_t mask;
  CPU_ZERO(&mask);
  CPU_SET(num, &mask);
  result = sched_setaffinity(0, sizeof(mask), &mask);
  if (result < 0) {
    printf("binding CPU fails\n");
    exit(1);
  }
}

/// Check a condition at runtime. If the condition is false, throw exception.
static inline void rt_assert(bool condition) {
  if (unlikely(!condition)) throw std::runtime_error("");
}


/* allocate the huge pages. */
inline char *alloc_raw_pages(int cnt, int size) {
  /*
   *  Don't touch the page since then allocator would not allocate the page
   * right now.
   */
  int flag = MAP_SHARED | MAP_ANONYMOUS;
  if (size == EACH_HUGEPAGE_SIZE) flag |= MAP_HUGETLB;
  char *ptr =
      (char *)mmap(NULL, (int64_t)cnt * size, PROT_READ | PROT_WRITE, flag, -1, 0);
  if (ptr == (char *)-1) {
    perror("alloc_raw_pages");
    return NULL;
  }
  return ptr;
}

inline uint16_t hash_by(uint16_t agtr, uint8_t appID) {
  // return (agtr + appID) % MAX_AGTR_COUNT;
  return agtr % MAX_AGTR_COUNT; 
}

// /* Returns the MAC Address Params: int iNetType - 0: ethernet, 1: Wifi char chMAC[6] - MAC Address in binary format Returns: 0: success -1: Failure */
// int getMACAddress(char chMAC[6])
// {
//     struct ifreq ifr;
//     int sock;
//     char* ifname = "enp178s0f0";
//     sock = socket(AF_INET, SOCK_DGRAM, 0);
//     strcpy(ifr.ifr_name, ifname);
//     ifr.ifr_addr.sa_family = AF_INET;
//     if (ioctl(sock, SIOCGIFHWADDR, &ifr) < 0) {
//         return -1;
//     }
//     memcpy(chMAC, ifr.ifr_hwaddr.sa_data, 6);
//     close(sock);
//     return 0;
// }
 
// /* Returns the interface IP Address Params: int iNetType - 0: ethernet, 1: Wifi char *chIP - IP Address string Return: 0: success / -1: Failure */
// int getIpAddress(char chIP[16])
// {
//     struct ifreq ifr;
//     int sock = 0;
//     sock = socket(AF_INET, SOCK_DGRAM, 0);
//     strcpy(ifr.ifr_name, "enp178s0f0");
//     if (ioctl(sock, SIOCGIFADDR, &ifr) < 0) {
//         strcpy(chIP, "0.0.0.0");
//         return -1;
//     }
//     sprintf(chIP, "%s", inet_ntoa(((struct sockaddr_in*)&(ifr.ifr_addr))->sin_addr));
//     close(sock);
//     return 0;
// }

#endif
