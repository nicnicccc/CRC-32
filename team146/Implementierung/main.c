#include <stdio.h>
#include <complex.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>
#include <stdint.h>
#include <getopt.h>
#include <errno.h>
#include <sys/stat.h>
#include <emmintrin.h> // Include the SSE2 header



uint32_t CRC32(size_t len, const char msg[len], uint32_t generator);
void help(void);
void benchmark(uint32_t iteration, size_t len, const char msg[len], uint32_t generator, int imp);
void test(size_t len, const char msg[len], uint32_t generator, int imp);


uint32_t CRC32_V2(size_t len, const char msg[len], uint32_t generator) {
    uint32_t result = 0;    // Initialize result variable to store CRC value
    for (size_t i = 0; i < len; i++) {  // Iterate through each character in the message
        result ^= (uint32_t)(msg[i] << 24); // XOR the current character with the result, left-shifted by 24 bits
        for (size_t j = 0; j < 8; j++) {    // Perform 8 iterations for each bit
            if ((result & 0x80000000)) {    // Check if the most significant bit (MSB) is 1
                result = (uint32_t)((result<<1)^generator); // Shift result left by 1 bit and XOR with generator
            } else{
                result <<=1;    // If MSB is 0, simply shift result left by 1 bit
            }
        }
    }
    return result;  // Return the calculated CRC value
}

void crc32TableFill(uint32_t* table, uint32_t generator) {
    int32_t result;
    for (int i = 0; i < 256; ++i) {
        result = (i << 24);
        for (uint8_t j = 8; j > 0; j--) {
            if ((result & 0x80000000)) {
                result = (result<<1)^generator;
            } else{
                result <<=1;
            }
        }
        table[i] = result;
    }
}

void crc32TableFillSIMD(uint32_t* table, uint32_t generator) {
    __m128i genpoll = _mm_set1_epi32(generator);
    for (int i = 0; i < 256; i += 4) {
        __m128i res = _mm_slli_epi32(_mm_set_epi32(i+3, i+2, i+1, i), 24);
        for (uint8_t j = 8; j > 0; j--) {
            __m128i msb = _mm_srai_epi32(res, 31);
            res = _mm_slli_epi32(res, 1);

            __m128i r = _mm_and_si128(msb, genpoll);
            res = _mm_xor_si128(res, r); 
        }
        _mm_storeu_si128((__m128i*)&table[i], res); 
    }
}

uint32_t CRC32_V1(size_t len, const char msg[len], uint32_t generator) {
    uint32_t result = 0;
    uint32_t tabel[256];
    crc32TableFill(tabel, generator);
    unsigned char byt;

    for (size_t i = 0; i < len; i++) {
        byt = msg[i] ^ (result >> 24);
        result = tabel[byt] ^ (result<<8);
    }
    return result;
}

uint32_t CRC32(size_t len, const char msg[len], uint32_t generator) {
    uint32_t result = 0;
    uint32_t tabel[256];
    crc32TableFillSIMD(tabel, generator);
    unsigned char byt;

    for (size_t i = 0; i < len; i++) {
        byt = msg[i] ^ (result >> 24);
        result = tabel[byt] ^ (result<<8);
    }
    return result;
}


static char* read_file(const char* input_file){
    char * string = NULL ;
    FILE * file ;

    if (!( file = fopen (input_file , "r") ) ) {
        perror ("Error opening file ") ;
        return NULL;
    }

    struct stat statbuf;
    if (fstat(fileno (file) , &statbuf)) {
        fprintf (stderr , "Error retrieving file stats \n") ;
        goto cleanup ;
    }
    if (!S_ISREG (statbuf.st_mode) || statbuf.st_size <= 0) {
        fprintf (stderr , "Error processing file : Not a regular file or invalid size \n") ;
        goto cleanup ;
    }

    if (!(string = malloc (statbuf.st_size + 1) ) ) {
        fprintf ( stderr , "Error reading file : Could not allocate enough memory \n") ;
        goto cleanup ;
    }

    if (fread (string , 1 , statbuf.st_size , file ) != ( size_t ) statbuf.st_size ) {
        fprintf (stderr , " Error reading file ") ;
        free (string ) ;
        string = NULL ;
        goto cleanup ;
    }

    string[statbuf.st_size] = '\0';

    cleanup :
    if (file)
        fclose(file);


    return string;
}

int main(int argc, char** argv) {
    if(argc == 1){
        fprintf(stderr, "No input or options given!\n");
        fprintf(stderr, "For help type ./main -h\n");
        return -1;
    }
    int option;

    char* input_file = NULL;
    int len;


    long imp = 0;
    uint32_t generatorpolynom = 0x4C11DB7;

    char* endptrV;
    char* endptrG;
    char* endptrB;

    int ftime = 0;
    int ftest = 0;
    int fimp = 0;
    int fgen = 0;
    int fhelp = 0;

    uint64_t iteration ;

    struct option long_options[] = {
            {"help", no_argument, 0, 'h'},
            {0, 0, 0, 0}
    };


    while ((option = getopt_long(argc, argv, "V:B::G:ht", long_options, NULL)) != -1) {
        switch (option) {
            case 'V':
                fimp++;
                if(fimp > 1) {
                    fprintf(stderr, "Option V may only be called once.\n");
                    return 0;
                }
                errno = 0;
                imp = strtol(optarg, &endptrV, 10);
                if(*endptrV != '\0') {
                    fprintf(stderr, "Invalid input format! Please enter a valid number!\n");
                    return 0;
                }
                if(imp < 0 || imp > 2) {
                    fprintf(stderr, "%s", "Ineligible number, it can only be 0, 1, 2!\n");
                    return 0;
                }

                break;
            case 'B':
                if(ftime > 0) {
                    fprintf(stderr, "Option B may only be called once.\n");
                    return 0;
                }
                if (optarg == NULL) {
                    iteration = 1;
                    ftime++;
                    //calculate 1 time, if there's no argument
                    //call the CRC32 function
                } else {
                    //calculate n times
                    errno = 0;
                    iteration = strtoull(optarg, &endptrB, 0);
                    if(*endptrB != '\0') {
                        fprintf(stderr, "Invalid input format! Please enter a valid number!\n");
                        return 0; 
                    }
                
                    if (errno == ERANGE || iteration > UINT32_MAX || iteration == 0) {
                        fprintf(stderr, "Number of iteration should be greater than 0 and smaller than UINT32_MAX value!\n");
                        return 0;
                    } else {
                        ftime++;
                        //call benchmark
                    }
                }
                break;
            case 'G':
                fgen++;
                if(fgen > 1) {
                    fprintf(stderr, "Option G may only be called once.\n");
                    return 0;
                }
                errno = 0;
                uint64_t genpol2 = strtoull(optarg, &endptrG, 0);
                if (*endptrG != '\0') {
                    fprintf(stderr, "Incorrect input, conversion error.\n");
                    return 0;
                }
                else if(errno == ERANGE || genpol2 > UINT32_MAX) {
                    fprintf(stderr, "Incorrect Generatorpolynom, out of range.\n");
                    return 0;
                }
                generatorpolynom = (uint32_t) genpol2;
                if(generatorpolynom == 0) {
                    fprintf(stderr, "Generatorpolynom must not be 0.\n");
                    return 0;
                }
                break;
            case 'h':
                fhelp++;
                if(fhelp > 1) {
                    fprintf(stderr, "Option h / help may only be called once.\n");
                    return 0;
                }
                break;
            case 't':
                if(ftest > 0) {
                    fprintf(stderr, "Option t may only be called once.\n");
                    return 0;
                }
                ftest++;
                break;
            default:
                fprintf(stderr, "Invalid argument.\n");
                return 0;
        }
    }

    if(fhelp == 1){
        help();
        return 0;
    }

    if(ftime == 1 && ftest == 1){
        fprintf(stderr, "Option t and B may not be called at the same time.\n");
        return 0;
    }

    if(optind < argc) {
        input_file = argv[optind];
        if (optind + 1 < argc) {
            fprintf(stderr, "Only one file is allowed to be processed.\n"); 
            return 0;
        }
    } else {
        fprintf(stderr, "No given file.\n");
        return 0;
    }

    char* string = read_file(input_file);

    if(string == NULL) return 0;
    uint64_t lenstr = strlen(string);
    printf("String Length = %lu\n", lenstr);
    printf("Generatorpolynom = 0x%x\n", generatorpolynom);



    if(ftest == 1){
        test(lenstr, string, generatorpolynom, imp);
        free(string);
        return 0;
    }

    if(ftime == 1 ) {
        benchmark(iteration, lenstr, string, generatorpolynom, imp);
    } else {
        if (imp == 0) {
            printf("Implementation with Lookup Table and SIMD, checksum = %x\n", CRC32(lenstr, string, generatorpolynom));
        } else if (imp == 1) {
            printf("Implementation with Lookup Table, checksum = %x\n", CRC32_V1(lenstr, string, generatorpolynom));
        }  else if (imp == 2) {
            printf("Naive-Implementation, checksum = %x\n", CRC32_V2(lenstr, string, generatorpolynom));
        } else {
            printf("No provided implementation.\n");
        }
    }
    free(string); //free memory allocation(prevents memleaks)

    return 0;
}

void help(void) {
    printf("%s\n", "================================================================================================================================================================");
    printf("%s\n", "Usage format  : ./main [options] <filename>");
    printf("%s\n", "Usage example : ./main -B10 -V1 <filename>");
    printf("%s\n", "Explanation   : take the message in a text file as an argument for the function CRC32 together with a generator polynomial ");
    printf("%s\n", "              (Default : 0x04C11DB7), and create a check sum.");
    printf("%s\n", "================================================================================================================================================================");
    printf("%s\n", "Positional Argument : ");
    printf("%s\n", "<filename> contains the message[len] which integrity should be checked");
    printf("%s\n", "----------------------------------------------------------------------------------------------------------------------------------------------------------------");
    printf("%s\n", "Optional Argument   : ");
    printf("%s\n", " -V<Number>     The possibilities of the Implementation. Default = V0 = main Implementation (Implementation with Lookup table and SIMD) ");
    printf("%s\n", "                                                                 = V1 = Implementation with Lookup table ");
    printf("%s\n", "                                                                 = V2 = Naive Implementation");
    printf("%s\n",  "For the Optional Argument of -B and -G, the possible input types are : 0x<Hexadecimal> / 0<Octadecimal> / <Decimal> ");
    printf("%s\n", " -B<Number>     If set, the runtime of the specified implementation is measured and printed to the console.");
    printf("%s\n", "                The optional argument <Number> of this option specifies the number of iterations of the function call.");
    printf("%s\n", "                Default : <Number> = 1");
    printf("%s\n", " -G<Number>     Generator Polynomial, Default : 0x04C11DB7 (IEEE802.3 Standard Generator Polynom)");
    printf("%s\n", " -t             Show the test cases of the program executed.");
    printf("%s\n", " -h             Show description of all the options of the program (this text) and usage examples are output and the program then exits.");
    printf("%s\n", " --help         Show description of all the options of the program (this text) and usage examples are output and the program then exits.");
    printf("%s\n", "================================================================================================================================================================");
}

__attribute__((optimize("O0")))void benchmark (uint32_t iteration, size_t len, const char msg[len], uint32_t generator, int imp) {
    struct timespec start;
    struct timespec end;
    uint32_t result = 0;
    switch (imp) {
        case 0 :
            clock_gettime(CLOCK_MONOTONIC, &start);
            //start time measurement
            for(int i = 0 ; i < iteration ; i++) {
                result = CRC32(len,msg,generator);
            }
            //end time measurement
            clock_gettime(CLOCK_MONOTONIC, &end);
            break;
        case 1 :
            clock_gettime(CLOCK_MONOTONIC, &start);
            //start time measurement
            //call the optimized method with lookup table
            for(int i = 0 ; i < iteration ; i++) {
                result = CRC32_V1(len,msg,generator);
            }
            //end time measurement
            clock_gettime(CLOCK_MONOTONIC, &end);
            break;
        case 2 :
            clock_gettime(CLOCK_MONOTONIC, &start);
            //start time measurement
            for(int i = 0; i < iteration; i++) {
                result = CRC32_V2(len,msg,generator);
            }
            //end time measurement
            clock_gettime(CLOCK_MONOTONIC, &end);
            break;

        default:
            fprintf(stderr, "No provided implementation.\n");
            return;
    }

    //calculate the average time needed to do 1 operation
    double time = end.tv_sec - start.tv_sec + (1e-9 * (end.tv_nsec - start.tv_nsec));
    double avgtime = (time / iteration);

    printf("Implementation version                     = %d\n", imp);
    printf("Time needed to do the operation %u times is = %f \n", iteration, time);
    printf("Average time needed for one operation is   = %f \n", avgtime);
    printf("Result (check sum) is                      = %x \n",result);
}

void test(size_t len, const char msg[len], uint32_t generator, int imp){
    uint32_t res;
    if(imp == 0) {
        res = CRC32(len, msg, generator);
    } else if (imp == 1) {
        res = CRC32_V1(len, msg, generator);
    } else if (imp == 2) {
        res = CRC32_V2(len, msg, generator);
    } else {
        fprintf(stderr, "No provided implementation.\n");
        return;
    }
    printf("Implementation version                = %d", imp);
/*
    printf("Input message                         = ");
*/
    /*for (int i = 0; i< strlen(msg); i++) {
        printf("%x", msg[i]);
    }*/

    int leng = strlen(msg) + 4;

    unsigned char* newStr;

    if (!(newStr = malloc (leng) ) ) {
        fprintf ( stderr , "Could not allocate enough memory \n") ;
        return;
    }

    size_t i = 0;
    size_t n = strlen(msg);
    printf("\nChecksum                              = %x", res);
/*
    printf("\nResult message with appended checksum = ");
*/
    for (; i < n - (n%16); i+=16) {
        __m128i simd = _mm_loadu_si128((__m128i_u*)(msg+i));
        _mm_storeu_si128((__m128i_u*)(newStr+i), simd);

    }
    for (; i < n; ++i) {
        newStr[i] = msg[i];
    }

    newStr[leng - 1] = res;
    newStr[leng - 2] = (res>>8);
    newStr[leng - 3] = (res>>16);
    newStr[leng - 4] = (res>>24);

    /*for (int i = 0; i < leng; i++) {
        printf("%x", newStr[i]);
    }*/

    uint32_t res1;
    if(imp == 0) {
        res1 = CRC32(leng, newStr, generator);
    } else if (imp == 1) {
        res1 = CRC32_V1(leng, newStr, generator);
    } else if (imp == 2) {
        res1 = CRC32_V2(leng, newStr, generator);
    }
    printf("\nChecksum of appended message          = %x\n", res1);
    free(newStr);
}