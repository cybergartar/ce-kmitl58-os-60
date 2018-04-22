#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <unistd.h>

#define SUPPLIER_NUM  5
#define CONSUMER_NUM  8
#define SUPPLY_LIMIT  100

char* gettime();
void* run_supplier(void* arg);
void* run_consumer(void* arg);

// arrays that keep product name and it's count
// to compare to config (explain later)
char* product_name[SUPPLIER_NUM];
unsigned int product_count = 0;

// threads of consumer and supplier
pthread_t consumer_thread[CONSUMER_NUM], supplier_thread[SUPPLIER_NUM];

// lock and cond var of supply and product name
pthread_mutex_t mutex[SUPPLIER_NUM], product_name_mutex;
pthread_cond_t cond[SUPPLIER_NUM], product_name_cond;

// active user of supply and product name
unsigned int active_user[SUPPLIER_NUM], active_product_name_editor = 0;

// supply arrays
unsigned int supply[SUPPLIER_NUM];

/**
    Main function of program, spawn supplier and
    consumer threads. also apply join function to them
    so the program won't terminate while threads are
    running

    @param none
    @return none
*/
int main() {
  unsigned int i;

  // initiate supply array, filling 0 to entire array
  memset(supply, 0, sizeof(supply));

  // spawn supplier threads
  for (i = 0; i < SUPPLIER_NUM; i++) {
    pthread_create(&(supplier_thread[i]), NULL, &run_supplier, (void*)i);
  }

  // spawn consumer threads
  for (i = 0; i < CONSUMER_NUM; i++) {
    pthread_create(&(consumer_thread[i]), NULL, &run_consumer, (void*)i);
  }

  // apply join to supplier threads
  for (i = 0; i < SUPPLIER_NUM; i++) {
    pthread_join(supplier_thread[i], NULL);
  }

  // apply join to supplier threads
  for (i = 0; i < CONSUMER_NUM; i++) {
    pthread_join(consumer_thread[i], NULL);
  }

  return 0;
}

/**
    get current time and convert to string for easy use

    @param none
    @return current datetime string in format
    Day Mnt DD HH:MM:SS YYYY
*/
char* gettime() {
  time_t utime;
  char *datetext;
  struct tm *timedetail;

  // get current time
  time(&utime);

  // convert to string
  timedetail = localtime(&utime);
  datetext = asctime(timedetail);
  datetext[strlen(datetext) - 1] = 0;
  return datetext;
}

/**
    run supplier threads, read config and keep doing
    forever

    @param supplier id (supplier number in filename - 1)
    @return NULL
*/
void* run_supplier(void* arg) {
  // convert void* to uint
  unsigned int supplier_id = (unsigned int)arg;
  unsigned int prod_id, base_intv, rept;

  // read config from file
  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  char fname[255];
  char ids[2];

  // create filenamae form supplier_id and open it
  fname[0] = 0;
  strcat(fname, "supplier");
  
  ids[0] = ((int)(supplier_id+1) + '0'), ids[1] = 0;
  strcat(fname, ids);
  strcat(fname, ".txt");

  fp = fopen(fname, "r");
  if (fp == NULL) {
    exit(EXIT_FAILURE);
  }

  // read first line, which should be product name
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;

  // register product name and get product id
  // the product name array should be access by
  // only one thread at a time to prevent
  // the duplicates of product name
  pthread_mutex_lock(&product_name_mutex);
  while(active_product_name_editor > 0) {
    pthread_cond_wait(&product_name_cond, &product_name_mutex);
  }
  active_product_name_editor++;
  pthread_mutex_unlock(&product_name_mutex);

  // if there is no product in array
  // then this product must be product id 0
  if (product_count == 0) {
    product_name[0] = malloc(sizeof(char) * 256);

    // place product name in array
    strcpy(product_name[0], line);
    product_count++;

    // set this thread's associated product id to 0
    prod_id = 0;
  } else {
    // else if there is product in array already
    // loop through existing product name and see
    // if product name in config matches the existed one
    int i;
    for (i = 0; i < product_count; i++) {

      // if it matches, this thread's product id is
      // the index of product name in array
      if (strcmp(product_name[i], line) == 0) {
        prod_id = i;
        break;
      }
    }

    // else if no match, insert new product name in array
    // and set this thread's product id to the newly added
    // product's index
    if (i == product_count) {
      product_name[i] = malloc(sizeof(char) * 256);
      strcpy(product_name[i], line);
      product_count++;
      prod_id = i;
    }
  }

  // unlock access to product name array
  pthread_mutex_lock(&product_name_mutex);
  active_product_name_editor--;
  pthread_cond_signal(&product_name_cond);
  pthread_mutex_unlock(&product_name_mutex);

  // read next line of config, which should be waiting interval
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  base_intv = (int)strtol(line, NULL, 10);

  // read the last line of config, which should be repeat time
  // before increasing interval
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  rept = (int)strtol(line, NULL, 10);

  // close file
  fclose(fp);
  if (line) {
    free(line);
  }

  unsigned int intv = base_intv, penalty = 0;
  unsigned int BUSY_FLAG = 0;

  // explanation of thread variables
  // prod_id: product index uses to access supply array
  // base_intv: the original interval read from config
  // rept: number of tries before increase wait interval
  // intv: the interval used in action whict may be increased
  // penalty: the counting var of number of tries
  // BUSY_FLAG: indicate there is busy when accessing supply array

  // keeps doing forever
  while(1) {

    // locks before access active_user array of product
    pthread_mutex_lock(&mutex[prod_id]);

    // if we detect someone is using this product
    // then set BUSY_FLAG and wait until not busy
    while (active_user[prod_id] > 0) {
      BUSY_FLAG = 1;
      pthread_cond_wait(&cond[prod_id], &mutex[prod_id]);
    }
    // set this product to using
    active_user[prod_id]++;
    pthread_mutex_unlock(&mutex[prod_id]);

    // if we access this product when busy
    if (BUSY_FLAG == 1) {
      // check if penalty reaches number of maximum tries
      if (penalty == rept) {

        // if yes, double the interval of cap it at 60 secs
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        // clear penalty
        penalty = 0;
        // notify user
        printf("\e[96m[S]*\e[39m %s#%u\tsupplier interval is adjusted to %u\n", product_name[prod_id], supplier_id, intv);
      }
      // count it as number of tries
      penalty++;
      // notify user
      printf("\e[96m[S]\e[39m %s %s#%u\tsupplier going to \e[93mwait\e[39m\t \e[93m\e[1m[BUSY]\e[0m\e[39m.  Penalty = %d\n", gettime(), product_name[prod_id], supplier_id, penalty);

      // clear BUSY_FLAG
      BUSY_FLAG = 0;

    } else if (supply[prod_id] == SUPPLY_LIMIT) { 
      // if the supply slot is full

      // check if penalty reaches number of maximum tries
      if (penalty == rept) {

        // if yes, double the interval of cap it at 60 secs
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        // clear penalty
        penalty = 0;
        // notify user
        printf("\e[96m[S]*\e[39m %s#%u\tsupplier interval is adjusted to %u\n", product_name[prod_id], supplier_id, intv);
      }
      // count it as number of tries
      penalty++;
      // notify user
      printf("\e[96m[S]\e[39m %s %s#%u\tsupplier going to \e[93mwait\e[39m\t \e[95m\e[1m[FULL]\e[0m\e[39m.  Penalty = %d\n", gettime(), product_name[prod_id], supplier_id, penalty);
      
    } else {
      // else, we can fill the product
      supply[prod_id]++;

      // reset penalty
      penalty = 0;

      // revert interval to config interval
      intv = base_intv;

      // notify user
      printf("\e[96m[S]\e[39m %s %s\e[96m#%u\tsupplied\e[39m 1 unit. stock after = %d\n", gettime(), product_name[prod_id], supplier_id, supply[prod_id]);
    }

    // unlock access to supply array of product
    pthread_mutex_lock(&mutex[prod_id]);
    active_user[prod_id]--;
    pthread_cond_signal(&cond[prod_id]);
    pthread_mutex_unlock(&mutex[prod_id]);

    // wati for next round
    sleep(intv);
  }

  return NULL;
}

/**
    run consumer threads, read config and keep doing
    forever

    @param consumer id (consumer number in filename - 1)
    @return NULL
*/
void* run_consumer(void* arg) {
  // convert void* to uint
  unsigned int consumer_id = (unsigned int)arg;
  unsigned int prod_id, base_intv, rept;

  // read config from file
  FILE* fp;
  char* line = NULL;
  size_t len = 0;
  ssize_t read;
  char fname[255];
  char ids[2];

  // create filenamae form consumer_id and open it
  fname[0] = 0;
  strcat(fname, "consumer");
  
  ids[0] = ((int)(consumer_id+1) + '0'), ids[1] = 0;
  strcat(fname, ids);
  strcat(fname, ".txt");

  fp = fopen(fname, "r");
  if (fp == NULL) {
    exit(EXIT_FAILURE);
  }

  // read first line, which should be product name
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;

  // register product name and get product id
  // the product name array should be access by
  // only one thread at a time to prevent
  // the duplicates of product name
  pthread_mutex_lock(&product_name_mutex);
  while(active_product_name_editor > 0) {
    pthread_cond_wait(&product_name_cond, &product_name_mutex);
  }
  active_product_name_editor++;
  pthread_mutex_unlock(&product_name_mutex);

  // if there is no product in array
  // then this product must be product id 0
  if (product_count == 0) {
    product_name[0] = malloc(sizeof(char) * 256);

    // place product name in array
    strcpy(product_name[0], line);
    product_count++;

    // set this thread's associated product id to 0
    prod_id = 0;
  } else {
    // else if there is product in array already
    // loop through existing product name and see
    // if product name in config matches the existed one
    int i;
    for (i = 0; i < product_count; i++) {

      // if it matches, this thread's product id is
      // the index of product name in array
      if (strcmp(product_name[i], line) == 0) {
        prod_id = i;
        break;
      }
    }

    // else if no match, insert new product name in array
    // and set this thread's product id to the newly added
    // product's index
    if (i == product_count) {
      product_name[i] = malloc(sizeof(char) * 256);
      strcpy(product_name[i], line);
      product_count++;
      prod_id = i;
    }
  }

  // unlock access to product name array
  pthread_mutex_lock(&product_name_mutex);
  active_product_name_editor--;
  pthread_cond_signal(&product_name_cond);
  pthread_mutex_unlock(&product_name_mutex);

  // read next line of config, which should be waiting interval
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  base_intv = (int)strtol(line, NULL, 10);

  // read the last line of config, which should be repeat time
  // before increasing interval
  read = getline(&line, &len, fp);
  line[strlen(line)-1] = 0;
  rept = (int)strtol(line, NULL, 10);

  // close file
  fclose(fp);
  if (line) {
    free(line);
  }

  unsigned int intv = base_intv, penalty = 0;
  unsigned int BUSY_FLAG = 0;

  // explanation of thread variables
  // prod_id: product index uses to access supply array
  // base_intv: the original interval read from config
  // rept: number of tries before increase wait interval
  // intv: the interval used in action whict may be increased
  // penalty: the counting var of number of tries
  // BUSY_FLAG: indicate there is busy when accessing supply array

  // keeps doing forever
  while(1) {

    // locks before access active_user array of product
    pthread_mutex_lock(&mutex[prod_id]);

    // if we detect someone is using this product
    // then set BUSY_FLAG and wait until not busy
    while (active_user[prod_id] > 0) {
      BUSY_FLAG = 1;
      pthread_cond_wait(&cond[prod_id], &mutex[prod_id]);
    }
    // set this product to using
    active_user[prod_id]++;
    pthread_mutex_unlock(&mutex[prod_id]);

    // if we access this product when busy
    if (BUSY_FLAG == 1) {
      // check if penalty reaches number of maximum tries
      if (penalty == rept) {

        // if yes, double the interval of cap it at 60 secs
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        // clear penalty
        penalty = 0;
        // notify user
        printf("\e[92m[C]*\e[39m %s#%u\tconsumer interval is adjusted to %u\n", product_name[prod_id], consumer_id, intv);
      }
      // count it as number of tries
      penalty++;
      // notify user
      printf("\e[92m[C]\e[39m %s %s#%u\tconsumer going to \e[93mwait\e[39m\t \e[93m\e[1m[BUSY]\e[0m\e[39m.  Penalty = %d\n", gettime(), product_name[prod_id], consumer_id, penalty);

      // clear BUSY_FLAG
      BUSY_FLAG = 0;

    } else if (supply[prod_id] == 0) { 
      // if the supply slot is full

      // check if penalty reaches number of maximum tries
      if (penalty == rept) {

        // if yes, double the interval of cap it at 60 secs
        intv = ((intv * 2) > 60) ? 60 : (intv * 2);
        // clear penalty
        penalty = 0;
        // notify user
        printf("\e[92m[C]*\e[39m %s#%u\tconsumer interval is adjusted to %u\n", product_name[prod_id], consumer_id, intv);
      }
      // count it as number of tries
      penalty++;
      // notify user
      printf("\e[92m[C]\e[39m %s %s#%u\tconsumer going to \e[93mwait\e[39m\t \e[91m\e[1m[EMPTY]\e[0m\e[39m. Penalty = %d\n", gettime(), product_name[prod_id], consumer_id, penalty);

    } else {
      // else, we can fill the product
      supply[prod_id]--;

      // reset penalty
      penalty = 0;

      // revert interval to config interval
      intv = base_intv;

      // notify user
      printf("\e[92m[C]\e[39m %s %s\e[92m#%u\tconsumed\e[39m 1 unit. stock after = %d\n", gettime(), product_name[prod_id], consumer_id, supply[prod_id]);
    }

    // unlock access to supply array of product
    pthread_mutex_lock(&mutex[prod_id]);
    active_user[prod_id]--;
    pthread_cond_signal(&cond[prod_id]);
    pthread_mutex_unlock(&mutex[prod_id]);

    // wati for next round
    sleep(intv);
  }

  return NULL;
}
