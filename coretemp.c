
         #include <sys/types.h>
         #include <sys/stat.h>
         #include <fcntl.h>
         #include <unistd.h>
         #include <string.h>
         #include <errno.h>
         #include <stdio.h>
#include <sys/types.h>
       #include <sys/processor.h>
       #include <sys/procset.h>
#include <kstat.h>

long getKStatNumber(kstat_ctl_t *kernelDesc, char *moduleName, char *recordName, char *fieldName);
char *getKStatString(kstat_ctl_t *kernelDesc, char *moduleName, char *recordName, char *fieldName);


         static const char devname[] = "/dev/cpu/self/msr";
	 uint64_t msr;
	 kstat_ctl_t *kernelDesc;
	 int cores = 1;

         /*ARGSUSED*/
         int
         main(int argc, char *argv[])
         {
                 int d;

		 if ((kernelDesc = kstat_open()) == NULL) {
		      perror("kstat_open");
		 } else {
			cores = getKStatNumber(kernelDesc, "cpu_info", "cpu_info0", "ncore_per_chip");
		 }

		 

                 if ((d = open(devname, O_RDONLY)) == -1) {
                         perror(devname);
                         return (1);
                 }
		 
		 // TjMAX
		 int tj_max = 100;
                 if(pread(d, &msr, sizeof(msr), 0x1a2) == sizeof(msr)) {
                      tj_max = (msr >> 16) & 0x7f;
                 }
		 // package temp
		 if(pread(d, &msr, sizeof(msr), 0x1b1) == sizeof(msr)) {
			 	int temp = tj_max - ((msr >> 16) & 0x7f);
			 	printf("Package temperature: %d\n", temp);
		 }
		 int i;
		 for(i = 0; i < cores; i++) {
			 processor_bind(P_LWPID, P_MYID, i, NULL);

			 int temp = -1;
			 if(pread(d, &msr, sizeof(msr), 0x19c) == sizeof(msr)) {
				 temp = tj_max - ((msr >> 16) & 0x7f);
			 }
		printf("Core %d temperature: %d\n", i, temp);
		 }

		(void)close(d);


		 return(0);

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
