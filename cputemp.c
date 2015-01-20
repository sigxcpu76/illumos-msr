#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <sys/processor.h>
#include <sys/procset.h>
#include <kstat.h>
#include <stdlib.h>

#define DEV_CPUID "/dev/cpu/self/cpuid"
#define DEV_MSR "/dev/cpu/self/msr"
#define MAX_CPUS 1024


typedef struct {
  uint32_t eax, ebx, ecx, edx;
} regs_t;

typedef struct {
  int cpu_index;
  int chip_id;
  int core_id;
} cpu_info_t;

long getKStatNumber(kstat_ctl_t *kernelDesc, char *moduleName, char *recordName, char *fieldName);
char *getKStatString(kstat_ctl_t *kernelDesc, char *moduleName, char *recordName, char *fieldName);
int read_msr_on_cpu(int cpu_index, uint32_t msr_index, uint64_t *result);
int read_msr(uint32_t msr_index, uint64_t *result);
int read_cpuid(uint32_t cpuid_func, regs_t *regs);
int read_cpuid_on_cpu(int cpu_index, uint32_t cpuid_func, regs_t *regs);
void temp_to_str(char *str, int temp);

int main(int argc, char *argv[1]) {

  regs_t regs;
  kstat_ctl_t *kstat;
  cpu_info_t cpus[MAX_CPUS];
  int cpu_count;
  int cpu_sockets = 0;

  if ((kstat = kstat_open()) == NULL) {
    perror("kstat_open");
    return (1);
  }

  int machine_readable = 0;
  int display_cpu = -1;
  if(argc > 1) {
    if(!strcmp(argv[1], "-p")) {
      machine_readable = 1;
    }
    if(argc > 2) {
      display_cpu = atoi(argv[2]);
    }
  }



  for(cpu_count = 0; cpu_count < MAX_CPUS; cpu_count++) {
    char record_name[128];
    sprintf(record_name, "cpu_info%d", cpu_count);
    int chip_id;
    if ((chip_id = getKStatNumber(kstat, "cpu_info", record_name, "chip_id")) < 0) {
      break;
    }
    int core_id = getKStatNumber(kstat, "cpu_info", record_name, "pkg_core_id");
    cpus[cpu_count].cpu_index = cpu_count;
    cpus[cpu_count].chip_id = chip_id;
    cpus[cpu_count].core_id = core_id;

    if(chip_id + 1 > cpu_sockets){
      cpu_sockets = chip_id + 1;
    }
  }


  if(!machine_readable) printf("Found %d CPU%s in %d socket%s\n", cpu_count, (cpu_count == 1) ? "" : "s", cpu_sockets, (cpu_sockets == 1) ? "" : "s");

  int cpu_socket, cpu_core, cpu_index;
  uint64_t msr;

  for(cpu_socket = 0; cpu_socket < cpu_sockets; cpu_socket++) {
    // find cores for current socket
    for(cpu_core = 0; cpu_core < MAX_CPUS; cpu_core++) {
      cpu_info_t *core_ptr = NULL;
      for(cpu_index = 0; cpu_index < cpu_count; cpu_index++) {
        cpu_info_t *cpu_ptr = &cpus[cpu_index];
        if((cpu_ptr->chip_id == cpu_socket) && (cpu_ptr->core_id == cpu_core)) {
          core_ptr = cpu_ptr;
          break;
        }
      }

      if(core_ptr) {
        read_cpuid_on_cpu(cpu_index, 6, &regs);
        int has_package_temp_monitor = (regs.eax >> 6) & 0x01;
        int has_thermal_monitoring = (regs.eax) & 0x01;

        if(has_thermal_monitoring) {
          // read TjMAX first
          int tj_max = 100;
          if(read_msr_on_cpu(cpu_index, 0x1a2, &msr) == 0) {
            tj_max = (msr >> 16) & 0x7f;
          }

          if(core_ptr->core_id == 0) {
            // this is the first core, so print package information, too
            int package_temp = -1;
            if(has_package_temp_monitor) {
              if(read_msr_on_cpu(cpu_index, 0x1b1, &msr) == 0) {
                package_temp = tj_max - ((msr >> 16) & 0x7f);
              }
            }
            if(!machine_readable) {
              printf("Socket #%d", core_ptr->chip_id);
              if(package_temp >= 0) {
                printf(" temp : %d \u00B0C\n", package_temp);
              } else {
                printf("\n");
              }
            }
          }
          // print core information
          int core_temp = -1;
          if(read_msr_on_cpu(cpu_index, 0x19c, &msr) == 0) {
            core_temp = tj_max - ((msr >> 16) & 0x7f);
          }
          if(!machine_readable) {
            printf("\tCore #%d", cpu_core);
            if(core_temp >= 0) {
              printf(" temp : %d \u00B0C\n", core_temp);
            } else {
              printf("\n");
            }
          } else {
            if(display_cpu == -1) {
              printf("%d %d\n", core_ptr->cpu_index, core_temp);
            } else if(display_cpu == core_ptr->cpu_index) {
              printf("%d\n", core_temp);
            }
          }
        }
      }
    }
  }


  kstat_close(kstat);

  return 0;

}

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

long getKStatNumber(kstat_ctl_t *kernelDesc, char *moduleName, 
     char *recordName, char *fieldName) {
  kstat_t *kstatRecordPtr;
  kstat_named_t *kstatFields;
  long value;
  int i;
       
  if ((kstatRecordPtr = kstat_lookup(kernelDesc, moduleName, -1, recordName)) ==
       NULL) {
     return(-1);
  }

  if (kstat_read(kernelDesc, kstatRecordPtr, NULL) < 0)
    return(-1);

  kstatFields = KSTAT_NAMED_PTR(kstatRecordPtr);

  for (i=0; i<kstatRecordPtr->ks_ndata; i++) {
    if (strcmp(kstatFields[i].name, fieldName) == 0) {
       switch(kstatFields[i].data_type) {
          case KSTAT_DATA_INT32:
               value = kstatFields[i].value.i32;
               break;
          case KSTAT_DATA_UINT32:
               value = kstatFields[i].value.ui32;
               break;
          case KSTAT_DATA_INT64:
               value = kstatFields[i].value.i64;
               break;
          case KSTAT_DATA_UINT64:
               value = kstatFields[i].value.ui64;
               break;
          default:
               value = -1;
       }
       return(value);
    }
  }
  return(-1);
}

/* Fetch string statistic from kernel */
char *getKStatString(kstat_ctl_t *kernelDesc, char *moduleName, 
     char *recordName, char *fieldName) {
  kstat_t *kstatRecordPtr;
  kstat_named_t *kstatFields;
  char *value;
  int i;
       
  if ((kstatRecordPtr = kstat_lookup(kernelDesc, moduleName, -1, recordName)) ==
       NULL) {
     return(NULL);
  }

  if (kstat_read(kernelDesc, kstatRecordPtr, NULL) < 0)
    return(NULL);

  kstatFields = KSTAT_NAMED_PTR(kstatRecordPtr);

  for (i=0; i<kstatRecordPtr->ks_ndata; i++) {
    if (strcmp(kstatFields[i].name, fieldName) == 0) {
       switch(kstatFields[i].data_type) {
          case KSTAT_DATA_CHAR:
               value = kstatFields[i].value.c;
               break;
          default:
               value = NULL;
       }
       return(value);
    }
  }
  return(NULL);
}
