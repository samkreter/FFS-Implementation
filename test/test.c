#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "../src/FonefiveFS.c"


#define assert(e) ((e) ? (true) : \
                   (fprintf(stderr,"%s,%d: assertion '%s' failed\n",__FILE__, __LINE__, #e), \
                    fflush(stdout), abort()))

/*

    int fs_format(const char *const fname);
    1   Normal
    2   NULL
    3   Empty string

    F15FS_t *fs_mount(const char *const fname);
    1   Normal
    2   NULL
    3   Empty string

    int fs_unmount(F15FS_t *fs);
    1   Normal
    2   NULL
    
*/

// SIZES AND CREATION/MOUNT/UNMOUNT
void basic_tests_a();


void basic_tests_b();


void basic_tests_c();


void basic_tests_d();

int main() {

    puts("Running autotests, sit back and relax, it'll be awhile...");

    basic_tests_a();

    puts("A tests passed...");

    basic_tests_b();

    puts("B tests passed...");

    //basic_tests_c();

    //puts("C tests passed...");

    //basic_tests_d();

    //puts("D tests passed...");

    puts("TESTS COMPLETE");

}

void basic_tests_a() {

    // I heavily suggest you assert your struct sizes here

    // FORMAT 1
    assert(fs_format("TESTFILE.f15fs") == 0);
    // FORMAT 2
    assert(fs_format(NULL) < 0);
    // FORMAT 3
    assert(fs_format("") < 0);

    // MOUNT 1
    F15FS_t *fs = fs_mount("TESTFILE.f15fs");
    assert(fs);
    // MOUNT 2
    assert(fs_mount(NULL) == NULL);
    // MOUNT 3
    assert(fs_mount("") == NULL);

    // UNMOUNT 1
    assert(fs_unmount(fs) == 0);
    // UNMOUNT 2
    assert(fs_unmount(NULL) < 0);

}


void basic_tests_b() {
    //test formating
    assert(fs_format("TESTFILE.f15fs") == 0);

    // test mounting
    F15FS_t *fs = fs_mount("TESTFILE.f15fs");
    assert(fs);

    //make sure everthing saves to the block store correctly
    fs->inodeTable[1].fname[0] = 't';
    char testMatch = fs->inodeTable[1].fname[0];

    printf("%c::::first time\n",fs->inodeTable[1].fname[0]);

    //unmount to save to file
    assert(fs_unmount(fs) == 0);
    
    //change the local var
    fs->inodeTable[1].fname[0] = 'y';
    printf("%c::::Changing local var\n",fs->inodeTable[1].fname[0]);
    
    //mount again
    fs = fs_mount("TESTFILE.f15fs");
    printf("%c::::after remount\n",fs->inodeTable[1].fname[0]);

    //make sure what goes in comes out 
    assert(testMatch == fs->inodeTable[1].fname[0]);
}


void basic_tests_c() {

}

void basic_tests_d() {


}
