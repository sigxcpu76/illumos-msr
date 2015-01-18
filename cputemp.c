#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/processor.h>
#include <sys/procset.h>

#define DEV_CPUID "/dev/cpu/self/cpuid"
#define DEV_MSR	"/dev/cpu/self/msr"


typedef struct {
  uint32_t eax, ebx, ecx, edx;
} regs_t;


int read_msr_on_cpu(int cpu_index, uint32_t msr_index, uint64_t *result) {
  int d;

  if((d = open(DEV_MSR, O_RDONLY)) == -1) {
    perror(DEV_MSR);
    return(errno);
  }
  processor_bind(P_LWPID, P_MYID, cpu_index, NULL);
  int res = (sizeof(uint64_t) == pread(d, result, sizeof(uint64_t), msr_index));
  processor_bind(P_LWPID, P_MYID, PBIND_NONE, NULL);
  close(d);
  if(res) {
    return (0);
  } else {
    return (1);
  }
}

int read_msr(uint32_t msr_index, uint64_t *result) {
  return read_msr_on_cpu(0, msr_index, result);
}


int read_cpuid_on_cpu(int cpu_index, uint32_t cpuid_func, regs_t *regs) {
  int d;

  if((d = open(DEV_CPUID, O_RDONLY)) == -1) {
    perror(DEV_CPUID);
    return (errno);
  }
  
  processor_bind(P_LWPID, P_MYID, cpu_index, NULL);
  int read_data = pread(d, regs, sizeof(*regs), cpuid_func);
  int saved_error = errno;
  processor_bind(P_LWPID, P_MYID, PBIND_NONE, NULL);

  close(d);

  if(read_data != sizeof(*regs)) {
    errno = saved_error;
    perror(DEV_CPUID);
    return(-1);
  }

  return(0);
}

int read_cpuid(uint32_t cpuid_func, regs_t *regs) {
  return read_cpuid_on_cpu(0, cpuid_func, regs);
}

void temp_to_str(char *str, int temp) {
  if(temp == -1) {
    sprintf(str, "n/a");
  } else {
    sprintf(str, "%d", temp);
  }
}

int main() {

  regs_t regs;
  /*
  // initialize cpu information first
  
  read_cpuid(1, &regs);

  int stepping = regs.eax & 0x0f;
  int model = ((regs.eax >> 4) & 0x0f) | (((regs.eax >> 16) & 0x0f) << 4);
  int family = ((regs.eax >> 8) & 0x0f) | (((regs.eax >> 20) & 0xff) << 4);
  int proc_type = (regs.eax >> 12) & 0x03;

  //printf("family=%d model=%d stepping=%d raw=%x\n", family, model, stepping, regs.eax);

  // check if CPU package has temp monitoring

  read_cpuid(6, &regs);
  int has_package_temp_monitor = (regs.eax >> 6) & 0x01;
  int has_thermal_monitoring = (regs.eax) & 0x01;

  if(!has_thermal_monitoring) {
    fprintf(stderr, "This CPU has no thermal monitoring");
    return(1);
  }
  //printf("has_package_temp_monitor=%d\n", has_package_temp_monitor);
  */
  int cores = sysconf(_SC_NPROCESSORS_ONLN);

  int i;
  uint64_t msr;

  for(i = 0; i < cores; i++) {
    read_cpuid_on_cpu(i, 6, &regs);
    int has_package_temp_monitor = (regs.eax >> 6) & 0x01;
    int has_thermal_monitoring = (regs.eax) & 0x01;

    
    int package_temp = -1;
    int core_temp = -1;
    int tj_max = -1;
    
    if(has_thermal_monitoring) {
      if(read_msr_on_cpu(i, 0x1a2, &msr) == 0) {
        tj_max = (msr >> 16) & 0x7f;  
      }
      if(has_package_temp_monitor) {
        if(read_msr_on_cpu(i, 0x1b1, &msr) == 0) {
          package_temp = tj_max - ((msr >> 16) & 0x7f);
        }
      }
      if(read_msr_on_cpu(i, 0x19c, &msr) == 0) {
        core_temp = tj_max - ((msr >> 16) & 0x7f);
      }

      
    }

    char str_core_temp[20];
    char str_package_temp[20];
    char str_tj_max[20];

    temp_to_str(str_core_temp, core_temp);
    temp_to_str(str_package_temp, package_temp);
    temp_to_str(str_tj_max, tj_max);

    printf("CPU %d: core_temp=%s package_temp=%s tj_max=%s\n", i, str_core_temp, str_package_temp, str_tj_max);

  }

  return (0);


}

