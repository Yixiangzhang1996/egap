/*
 * Induced Suffix Sorting and LCP array construction for String Collections
 *
 * Authors: Felipe A. Louza, Simon Gog, Guilherme P. Telles
 * contact: louza@ic.unicamp.br
 * 
 * version 1.1
 * 01/09/2015
 *
 * Modifications by Giovanni Manzini 12/01/17
 */

#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include "lib/file.h"
#include "lib/suffix_array.h"
#include "lib/lcp_array.h"
#include "src/malloc_count.h"
#include "src/gsacak.h"


#ifndef DEBUG
  #define DEBUG   0
#endif

#define MB 1048576
#define KB 1024

#define WORD (size_t)(pow(256,sizeof(int_t))/2.0)

void usage(char *name){
  printf("\n\tUsage: %s [options] FILE N\n\n",name);
  puts("Computes SA (and optionally LCP array) for the first N sequences of a");
  puts("collection using algorithm gSACA-K from Louza et al. DCC 16 paper. ");
  puts("Sequences from FILE are extracted according to FILE's");
  puts("extension; currently supported extensions are: .txt .fasta .fastq\n");
  puts("Available options:");
  puts("\t-h      this help message");
  puts("\t-m RAM  available memory in MB (def: no limit)");
  puts("\t-o OUT  base name for output files (def: FILE)");
  puts("\t-l      compute LCP array as well (use only with option -s)");
  puts("\t-c      check SA and LCP");
  puts("\t-s      output SA (ext: .sa) and possibly LCP (ext: .sa_lcp)");
  puts("\t-b      output BWT (ext: .bwt)");
  puts("\t-r      output RLE(BWT) (ext: .rle.bwt)");
  puts("\t-g D    output LCP in gap format D bytes per entry (ext: .D.lcp)");
  puts("\t-x      extract individual input files and stop");
  puts("\t-X      convert input to raw+len format (ext: .cat .len) and stop");
  // puts("\t-L    lengths of the input sequences in FILE.len (no separator)");
  puts("\t-v      verbose output (more v's for more verbose)\n");
  printf("sizeof(int): %zu bytes\n", sizeof(int_t));
  printf("Max text size: %zu\n", WORD);
  exit(EXIT_FAILURE);
}

/*******************************************************************/

int main(int argc, char** argv){
  extern char *optarg;
  extern int optind, opterr, optopt;
  
  // parse command line
  int VALIDATE=0, OUTPUT=0, LCP_COMPUTE=0;
  int_t k=0;
  int Verbose=0, OutputGapLcp=0, OutputBwt=0, Extract=0, c; // len_file=0;
  char *c_file=NULL, *outfile=NULL;
  size_t RAM=0;

  while ((c=getopt(argc, argv, "cslvXbrg:hm:o:")) != -1) {
    switch (c) 
      {
      case 'c':
        VALIDATE=1; break;          // validate output
      case 's':
        OUTPUT=1; break;            // output SA possibly combined with LCP
      case 'l':
        LCP_COMPUTE=1;  break;      // compute LCP 
      case 'v':
        Verbose++; break;
      case 'X':                     // TO BE TESTED (not sure if useful)
        Extract+=2; break;          // extract input sequences and concatenate them into a single file   
      case 'b':
        OutputBwt+=1; break;        // output BWT  
      case 'r':
        OutputBwt+=2; break;        // output RLE(BWT)
      case 'g':
        OutputGapLcp=atoi(optarg); LCP_COMPUTE=1; break;  // compute LCP and output in Gap format 
      case 'h':
        usage(argv[0]); break;       // show usage and stop
      case 'm':
        RAM=(size_t)atoi(optarg)*MB; break;
      case 'o':
        outfile = optarg; break;     // output file base name  
      case '?':
        exit(EXIT_FAILURE);
      }
  }

  if(optind+2==argc) {
    c_file=argv[optind++];
    k = (int_t) atoi(argv[optind++]);
  }
  else  usage(argv[0]);
  // if no outfile base name was givem use the input file name 
  if(outfile==NULL)
    outfile= c_file; 

  if(OutputGapLcp<0 || OutputGapLcp >= 8) {
    puts("Invalid lcp size!! Must be between 1 and 7\n");
    usage(argv[0]);
  }
  if(OutputGapLcp > sizeof(int_t)) {
    puts("Invalid lcp size!! Use gsais-64 to use 8-byte LCPs\n");
    usage(argv[0]);
  }
  
  // inits 
  time_t t_start=0, t_total=0;
  clock_t c_start=0, c_total=0;
  int_t i;

  printf("##\n");
  if(RAM){
    if(RAM<pow(2,20)) printf("RAM = %.2lf KB\n", RAM/pow(2,10));
    else if (RAM>=pow(2,20) && RAM<pow(2,30)) printf("RAM = %.2lf MB\n", RAM/pow(2,20));
    else printf("RAM = %.2lf GB\n", (double)RAM/pow(2,30));
    //fprintf(stderr,"RAM = %.2lf GB\n", (double)RAM/pow(2,30));
  }
  else printf("RAM = unlimited\n");
  
  int arrays = 1;
  if(LCP_COMPUTE) arrays++;
 
  size_t chunk_size = I_MAX/(sizeof(int_t)*arrays+1.0);
  if(RAM) chunk_size = RAM/(sizeof(int_t)*arrays+1.0);

  printf("max(chunk) = %lu symbols\n", chunk_size);

  if(chunk_size>WORD){
    fprintf(stderr, "##\nERROR: Partition larger than %.1lf GB (%.1lf GB)\nPlease build: make clean; make compile-64; and use gsais-64\n##\n", WORD/pow(2,30), (double)chunk_size/pow(2,30));
    exit(0);
  }

  size_t n=0;
 // int_t sum_n=0, total_n=1;
 // uint_t r=0;

  FILE* f_in = file_open(c_file, "rb");
  if(!f_in) return 0;

/**/
  //number of chunks
  int_t chunks = 0;
  //K[i] stores the number of strings into chunk C_i
  int_t* K = file_count_multiple(c_file, &k, chunk_size, &chunks, &n, f_in);

  printf("K = %" PRIdN "\n", k);
  printf("N = %zu\n", n+1);

  if(n>WORD){
    fprintf(stderr, "##\nERROR: INPUT larger than %.1lf GB (%.1lf GB)\nPlease build: make clean; make compile-64; and use gsais-64\n##\n", WORD/pow(2,30), n/pow(2,30));
    exit(0);
  }

  printf("CHUNKS = %" PRIdN "\n", chunks);
  printf("sizeof(int) = %zu bytes\n", sizeof(int_t));
  printf("##\n");
  //for(i=0; i<chunks; i++) printf("K[%" PRIdN "] = %" PRIdN "\n", i, K[i]);
/**/

  int_t b=0;
  time_start(&t_total, &c_total);

  if(Verbose){
    printf("CHUNK\tSTRINGS\tLENGHT\n");
  }

  // files used for multi BWT, LCP and their lengths 
  FILE *f_cat = NULL, *f_len = NULL, *f_bwt = NULL, *f_lcp = NULL;
  FILE *f_size = NULL; //size of each chunk

  if(Extract>1){
    char s[500]; 
    snprintf(s,500,"%s.%" PRIdN ".cat",outfile,k);
    f_cat=fopen(s,"wb");
    snprintf(s,500,"%s.%" PRIdN ".cat.len",outfile,k);
    f_len=fopen(s,"wb");
  }
  
  if(OutputBwt) {
    char s[500]; 
    if(OutputBwt==1) snprintf(s,500,"%s.bwt",outfile); 
    else snprintf(s,500,"%s.rle.bwt",outfile);
    f_bwt = file_open(s, "wb");
    snprintf(s,500,"%s.size",outfile);
    f_size = file_open(s, "wb");
  }

  if(OutputGapLcp){
    char s[500]; 
    snprintf(s,500,"%s.%d.lcp",outfile,OutputGapLcp);
    f_lcp = file_open(s, "wb");
  }

  // processing of individual chunks 
  for(b=0; b<chunks; b++){

    unsigned char **R;
    size_t len=0;

    // disk access
    //if(len_file==0)
    R = (unsigned char**) file_load_multiple_chunks(c_file, K[b], &len, f_in);
   
    if(!R){
      fprintf(stderr, "Error: less than %" PRIdN " strings in %s\n", K[b], c_file);
      return 0;
    }
    // now R[0] ... R[K[b]-1] contains the input documents  
    if(Verbose)
      printf("%" PRIdN "\t%" PRIdN "\t%lu\n", b, K[b], len);
        
    if(Extract>1) {   // save documents in chunk b in raw cat+len forma
      for(i=0;i<K[b];i++) {
        uint64_t j = 1 + strlen((char *)R[i]);
        #if  __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
          size_t z = fwrite(&j,4,1,f_len);
          assert(z==1); 
        #else
          fputc(j,f_len); fputc(j>>8,f_len); fputc(j>>16,f_len); fputc(j>>24,f_len);
        #endif
        size_t r = fwrite((char *)R[i],1,strlen((char *)R[i]),f_cat);
        if(r!=strlen((char *)R[i])) die(__func__);
        if(fputc(0,f_cat)!=0) {
          perror("Error writing EOS char"); die(__func__);
        }
      }
      //free memory
      for(i=0; i<K[b]; i++) free(R[i]);
      free(R);
      continue; // go to next chunk
    }

    // compute generalized SA for string collection
    //concatenate strings R[i] to str
    unsigned char *str = NULL;
    str = cat_char(R, K[b], &len);
    #if DEBUG
      int_t i;
      for(i=0;i<min(10,len); i++)
         printf("%" PRIdN ") %d\n", i, str[i]);
      printf("\n");
    #endif

    #if DEBUG
      printf("R:\n");
      for(i=0; i<min(5,K[b]); i++)
        printf("%" PRIdN ") %s (%zu)\n", i, R[i], strlen((char*)R[i]));
    #endif
  
    //free memory
    for(i=0; i<K[b]; i++) free(R[i]);
    free(R);

    // alloc and init SA 
    int_t *SA = (int_t*) malloc(len*sizeof(int_t));
    for(i=0; i<len; i++) SA[i]=0;
    int_t depth=0;
    // alloc and init LCP if necessary  
    int_t *LCP = NULL;  
    if(LCP_COMPUTE){
      LCP = (int_t*) malloc(len*sizeof(int_t));
      for(i=0; i<len; i++) LCP[i]=0;
    }
    
    if(Verbose)
      time_start(&t_start, &c_start);
  
    // computation of SA and possibly LCP
    if(LCP_COMPUTE){
      depth = gsacak((unsigned char*)str, (uint_t*)SA, LCP, NULL, len);
    }
    else{
      depth = gsacak((unsigned char*)str, (uint_t*)SA, NULL, NULL, len);
    }
    if(Verbose)
      fprintf(stderr,"gsacak returned depth: %"PRIdN"\n", depth);
    if(Verbose)
      fprintf(stderr,"%.6lf\n", time_stop(t_start, c_start));
  
    // output BWT  
    if(OutputBwt) {
      int c; int_t i;
      for(i=0; i<len; i++) {
        if(i==0)
          assert(SA[i]==len-1);
        else {
          c = bwt(SA[i],str);
        if(OutputBwt>1){ //RLE for DNA sequences
        
          unsigned char run=1;
          
          while(i+1<len && bwt(SA[i+1],str)==c && run<32){
            run++;i++;
          }
          
          #if DEBUG
            printf("<%c, %d> = ", c, run);
          #endif
          c = rle(c, run);
          #if DEBUG
            printf("%d\n", c);
          #endif
          }
          int err = fputc(c,f_bwt);
          if(err==EOF) die(__func__);
        }
      }
      // write BWT size to file 
      size_t len1 = len-1;
      fwrite(&len1,sizeof(size_t), 1, f_size);
    }
  
    if(Verbose>2) {
      if(LCP_COMPUTE) lcp_array_print((unsigned char*)str, SA, LCP, min(20,len), sizeof(char)); 
      else suffix_array_print((unsigned char*)str, SA, min(10,len), sizeof(char));
    }
  
    // validate 
    if(VALIDATE){
      if(!suffix_array_check((unsigned char*)str, SA, len, sizeof(char), 1)) printf("isNotSorted!!\n");//compares until the separator=1
      else printf("isSorted!!\ndepth = %" PRIdN "\n", depth);
      if(LCP_COMPUTE){
        if(!lcp_array_check_phi((unsigned char*)str, SA, LCP, len, sizeof(char), 1)) printf("isNotLCP!!\n");
        else printf("isLCP!!\n");
      }
    }
  
    // output SA alone or SA&LCP together
    if(OUTPUT){
      char tmp[500]; 
      if(LCP_COMPUTE) snprintf(tmp,500,"%" PRIdN ".sa_lcp",b);
      else snprintf(tmp,500,"%" PRIdN ".sa",b);

      if(LCP_COMPUTE) lcp_array_write(SA, LCP, len, outfile, tmp);
      else suffix_array_write(SA, len, outfile, "sa");
    }

    if(OutputGapLcp){
      uint64_t c; int_t i;
      uint64_t lcp_limit = (1LL << (8*OutputGapLcp))-1;
      for(i=1;i<len;i++) {  
        c = LCP[i];
        if(c>lcp_limit) {
          fprintf(stderr,"   !!! LCP entry larger than %"PRId64"\n", lcp_limit);
          fprintf(stderr,"   !!! Re-run using more bytes per LCP entry. Exiting...\n");
          exit(EXIT_FAILURE);
        }
        fwrite(&c,OutputGapLcp, 1, f_lcp);
      }
    }
    // free SA (LCP) and concatenated input collection  
    free(SA);
    if(LCP_COMPUTE) free(LCP);
    free(str);
    
  } // end chunks loop 

  fclose(f_in);
  free(K);

  printf("total:\n");
  fprintf(stderr,"%.6lf\n", time_stop(t_total, c_total));
  
  if(Extract>1){ 
    fclose(f_len);
    fclose(f_cat);
  }

  if(OutputBwt){
   fclose(f_bwt);
   fclose(f_size);
  }

  if(OutputGapLcp){
    fclose(f_lcp);
  }

  return 0;
}