#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define SUPPLIER_NUM  2
#define CONSUMER_NUM  3
#define SUPPLY_LIMIT  100

char* gettime();

char* product_name[SUPPLIER_NUM];
unsigned int product_count = 0;
pthread_t consumer_thread[CONSUMER_NUM], supplier_thread[SUPPLIER_NUM];
pthread_mutex_t mutex[SUPPLIER_NUM], product_name_mutex;
pthread_cond_t cond[SUPPLIER_NUM], product_name_cond;
unsigned int active_user[SUPPLIER_NUM], active_product_name_editor = 0;
unsigned int supply[SUPPLIER_NUM];

void* run_supplier(void* arg) {
  unsigned int supplier_id = (unsigned int)arg;
  unsigned int prod_id, base_intv, rept;

  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  char fname[255];
  char ids[2];

  fname[0] = 0;
  strcat(fname, "supplier");
  
  ids[0] = ((int)(supplier_id+1) + '0'), ids[1] = 0;
  strcat(fname, ids);
  strcat(fname, ".txt");

  fp = fopen(fname, "r");
  if (fp == NULL) {
    exit(EXIT_FAILURE);
  }
  //----------------------------------------------
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;

  pthread_mutex_lock(&product_name_mutex);
  while(active_product_name_editor > 0) {
    pthread_cond_wait(&product_name_cond, &product_name_mutex);
  }
  active_product_name_editor++;
  pthread_mutex_unlock(&product_name_mutex);

  if (product_count == 0) {
    product_name[0] = malloc(sizeof(char) * 256);
    strcpy(product_name[0], line);
    product_count++;
    prod_id = 0;
  } else {
    int i;
    for (i = 0; i < product_count; i++) {
      if (strcmp(product_name[i], line) == 0) {
        prod_id = i;
        break;
      }
    }

    if (i == product_count) {
      product_name[i] = malloc(sizeof(char) * 256);
      strcpy(product_name[i], line);
      product_count++;
      prod_id = i;
    }
  }

  pthread_mutex_lock(&product_name_mutex);
  active_product_name_editor--;
  pthread_cond_signal(&product_name_cond);
  pthread_mutex_unlock(&product_name_mutex);

  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  base_intv = (int)strtol(line, NULL, 10);

  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  rept = (int)strtol(line, NULL, 10);

  fclose(fp);
  if (line) {
    free(line);
  }

  unsigned int intv = base_intv, penalty = 0;
  unsigned int BUSY_FLAG = 0;

  while(1) {
    pthread_mutex_lock(&mutex[prod_id]);
    while (active_user[prod_id] > 0) {
      BUSY_FLAG = 1;
      pthread_cond_wait(&cond[prod_id], &mutex[prod_id]);
    }
    active_user[prod_id]++;
    pthread_mutex_unlock(&mutex[prod_id]);
    if (BUSY_FLAG == 1) {
      BUSY_FLAG = 0;
      if (penalty == rept) {
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        penalty = 0;
        printf("\e[96m[ S]*\e[39m %s#%u\tsupplier interval is adjusted to %u\n", product_name[prod_id], supplier_id, intv);
      }
      penalty++;
      printf("\e[96m[ S]\e[39m %s %s#%u\tsupplier going to \e[93mwait\e[39m\t \e[93m\e[1m[BUSY]\e[0m\e[39m.  Penalty = %d\n", gettime(), product_name[prod_id], supplier_id, penalty);
      
    } else if (supply[prod_id] == SUPPLY_LIMIT) { 
      if (penalty == rept) {
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        penalty = 0;
        printf("\e[96m[ S]*\e[39m %s#%u\tsupplier interval is adjusted to %u\n", product_name[prod_id], supplier_id, intv);
      }
      penalty++;
      printf("\e[96m[ S]\e[39m %s %s#%u\tsupplier going to \e[93mwait\e[39m\t \e[95m\e[1m[FULL]\e[0m\e[39m.  Penalty = %d\n", gettime(), product_name[prod_id], supplier_id, penalty);
      
    } else {
      supply[prod_id]++;
      penalty = 0;
      intv = base_intv;
      printf("\e[96m[ S]\e[39m %s %s\e[96m#%u\tsupplied\e[39m 1 unit. stock after = %d\n", gettime(), product_name[prod_id], supplier_id, supply[prod_id]);
    }
    pthread_mutex_lock(&mutex[prod_id]);
    active_user[prod_id]--;
    pthread_cond_signal(&cond[prod_id]);
    pthread_mutex_unlock(&mutex[prod_id]);
    sleep(intv);
  }

  return NULL;
}

void* run_consumer(void* arg) {
  unsigned int consumer_id = (unsigned int)arg;
  unsigned int prod_id, base_intv, rept;

  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  char fname[255];
  char ids[2];

  fname[0] = 0;
  strcat(fname, "consumer");
  
  ids[0] = ((int)(consumer_id+1) + '0'), ids[1] = 0;
  strcat(fname, ids);
  strcat(fname, ".txt");

  fp = fopen(fname, "r");
  if (fp == NULL) {
    exit(EXIT_FAILURE);
  }
  //----------------------------------------------
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;

  pthread_mutex_lock(&product_name_mutex);
  while(active_product_name_editor > 0) {
    pthread_cond_wait(&product_name_cond, &product_name_mutex);
  }
  active_product_name_editor++;
  pthread_mutex_unlock(&product_name_mutex);

  if (product_count == 0) {
    product_name[0] = malloc(sizeof(char) * 256);
    strcpy(product_name[0], line);
    product_count++;
    prod_id = 0;
  } else {
    int i;
    for (i = 0; i < product_count; i++) {
      if (strcmp(product_name[i], line) == 0) {
        prod_id = i;
        break;
      }
    }

    if (i == product_count) {
      product_name[i] = malloc(sizeof(char) * 256);
      strcpy(product_name[i], line);
      product_count++;
      prod_id = i;
    }
  }

  pthread_mutex_lock(&product_name_mutex);
  active_product_name_editor--;
  pthread_cond_signal(&product_name_cond);
  pthread_mutex_unlock(&product_name_mutex);

  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  base_intv = (int)strtol(line, NULL, 10);

  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  rept = (int)strtol(line, NULL, 10);

  fclose(fp);
  if (line) {
    free(line);
  }

  unsigned int intv = base_intv, penalty = 0;
  unsigned int BUSY_FLAG = 0;

  while(1) {
    pthread_mutex_lock(&mutex[prod_id]);
    while (active_user[prod_id] > 0) {
      BUSY_FLAG = 1;
      pthread_cond_wait(&cond[prod_id], &mutex[prod_id]);
    }
    active_user[prod_id]++;
    pthread_mutex_unlock(&mutex[prod_id]);
    if (BUSY_FLAG == 1) {
      BUSY_FLAG = 0;
      if (penalty == rept) {
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        penalty = 0;
        printf("\e[92m[C ]*\e[39m %s#%u\tconsumer interval is adjusted to %u\n", product_name[prod_id], consumer_id, intv);
      }
      penalty++;
      printf("\e[92m[C ]\e[39m %s %s#%u\tconsumer going to \e[93mwait\e[39m\t \e[93m\e[1m[BUSY]\e[0m\e[39m.  Penalty = %d\n", gettime(), product_name[prod_id], consumer_id, penalty);

    } else if (supply[prod_id] == 0) { 
      if (penalty == rept) {
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        penalty = 0;
        printf("\e[92m[C ]*\e[39m %s#%u\tconsumer interval is adjusted to %u\n", product_name[prod_id], consumer_id, intv);
      }
      penalty++;
      printf("\e[92m[C ]\e[39m %s %s#%u\tconsumer going to \e[93mwait\e[39m\t \e[91m\e[1m[EMPTY]\e[0m\e[39m. Penalty = %d\n", gettime(), product_name[prod_id], consumer_id, penalty);

    } else {
      supply[prod_id]--;
      intv = base_intv;
      penalty = 0;
      printf("\e[92m[C ]\e[39m %s %s\e[92m#%u\tconsumed\e[39m 1 unit. stock after = %d\n", gettime(), product_name[prod_id], consumer_id, supply[prod_id]);
    }
    pthread_mutex_lock(&mutex[prod_id]);
    active_user[prod_id]--;
    pthread_cond_signal(&cond[prod_id]);
    pthread_mutex_unlock(&mutex[prod_id]);
    sleep(intv);
  }

  return NULL;
}

int main() {
  unsigned int i;
  memset(supply, 0, sizeof(supply));

  for (i = 0; i < SUPPLIER_NUM; i++) {
    pthread_create(&(supplier_thread[i]), NULL, &run_supplier, (void*)i);
  }

  for (i = 0; i < CONSUMER_NUM; i++) {
    pthread_create(&(consumer_thread[i]), NULL, &run_consumer, (void*)i);
  }

  for (i = 0; i < SUPPLIER_NUM; i++) {
    pthread_join(supplier_thread[i], NULL);
  }

  for (i = 0; i < CONSUMER_NUM; i++) {
    pthread_join(consumer_thread[i], NULL);
  }

  return 0;
}

char* gettime() {
  time_t utime;
  char *datetext;
  struct tm *timedetail;
  time(&utime);
  timedetail = localtime(&utime);
  datetext = asctime(timedetail);
  datetext[strlen(datetext) - 1] = 0;
  return datetext;
}
