/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _POSIX_C_SOURCE 200809L
#include <infiltratr/filesystem_module_v2.h>
#include <assert.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#define SIZE 184320u
#define BASE 4608u
#define LENGTH 17003u
static uint8_t image[SIZE],proof[LENGTH],actual[LENGTH];
struct context{int fd,fail;unsigned sector,allocs;};
static int rd(void *p,uint64_t off,void *out,uint32_t n){
 struct context*c=p;assert(n&&n<=4096u&&!(off%c->sector)&&!(n%c->sector)&&off<=SIZE&&n<=SIZE-off);
 if(c->fail)return -1;
 if(c->fd>=0)return pread(c->fd,out,n,(off_t)off)==(ssize_t)n?0:-1;
 memcpy(out,image+off,n);return 0;
}
static void *allocate(void*p,size_t n){struct context*c=p;void*m=malloc(n);if(m)c->allocs++;return m;}
static void release(void*p,void*m){struct context*c=p;assert(m&&c->allocs);c->allocs--;free(m);}
static void header(void){
 static const uint8_t spec[]={0,0,40,9,2,1,3,2};
 image[0]=0xeb;memcpy(image+0x2b,"CP/M",4);memcpy(image+0x33,"DSK",3);memcpy(image+0x7c,"CP/M",4);memcpy(image+128,spec,8);
}
static void record(unsigned slot,unsigned user,const char *name,unsigned ext,unsigned bytes){
 uint8_t*p=image+BASE+slot*32;memset(p,0,32);p[0]=(uint8_t)user;memcpy(p+1,name,11);p[12]=(uint8_t)ext;
 p[13]=(uint8_t)(bytes%128);p[15]=(uint8_t)((bytes+127)/128);
}
static void fixture(void){
 memset(image,0,sizeof(image));header();memset(image+BASE,0xe5,2048);
 record(0,0,"PROOF   BIN",0,16384);record(1,0,"PROOF   BIN",1,LENGTH-16384);
 record(2,3,"NOTE    TXT",0,3);
 for(unsigned i=0;i<16;i++){unsigned b=2+i*2;image[BASE+16+i]=(uint8_t)b;memcpy(image+BASE+b*1024,proof+i*1024,1024);}
 image[BASE+32+16]=3;memcpy(image+BASE+3*1024,proof+16384,LENGTH-16384);
 image[BASE+64+16]=5;memcpy(image+BASE+5*1024,"yes",3);
}
static void expect(const IfsFsCoreOpsV2*core,IfsFsHostRuntimeV2*h,struct context*c,int want){
 uint32_t variant=99;int rc=core->probe(h,&variant);if(rc!=want)fprintf(stderr,"probe expected %d got %d\n",want,rc);assert(rc==want&&variant==(want==0?1u:0u)&&!c->allocs);
}
int main(int argc,char **argv){
 assert(argc==2||argc==4);
 if(argc==4){int fd=open(argv[3],O_RDONLY);assert(fd>=0&&read(fd,proof,LENGTH)==LENGTH&&!close(fd));}
 else for(unsigned i=0;i<LENGTH;i++)proof[i]=(uint8_t)(i*31u+i/7u);
 void *lib=dlopen(argv[1],RTLD_NOW|RTLD_LOCAL);assert(lib);
 IfsFsModuleApiEntryV2 get=(IfsFsModuleApiEntryV2)dlsym(lib,IFS_FS_COMMON_EXPORT_V2);assert(get);
 const IfsFsModuleApiV2*a=get();assert(ifs_fs_api_v2_valid(a,"cpm"));
 const IfsFsCoreOpsV2*core=(const void*)ifs_fs_group_v2(a,IFS_FS_GROUP_CORE);
 assert(core&&core->mount&&core->read&&!core->write&&!ifs_fs_group_v2(a,IFS_FS_GROUP_DEFRAG));
 struct context c={.fd=-1};
 if(argc==4){struct stat st;c.fd=open(argv[2],O_RDONLY);assert(c.fd>=0&&!fstat(c.fd,&st)&&st.st_size==SIZE);}
 IfsFsHostRuntimeV2 h={.abi_version=2,.structure_bytes=sizeof(h),.capabilities=IFS_FS_HOST_READ_ONLY,.volume_bytes=SIZE,.maximum_transfer_bytes=4096,.context=&c,.read_at=rd,.allocate=allocate,.release=release};
 for(c.sector=512;c.sector<=4096;c.sector*=8){
  h.logical_sector_bytes=h.physical_sector_bytes=c.sector;
  if(argc==2)fixture();
  expect(core,&h,&c,0);
  void*v=NULL;IfsFsVolumeInfoV2 info={.structure_bytes=sizeof(info)};
  assert(core->mount(&h,&v,&info)==IFS_FS_ACCESS&&!v);h.capabilities|=IFS_FS_HOST_EXCLUSIVE;
  assert(core->mount(&h,&v,&info)==0&&v&&info.block_bytes==1024&&info.free_bytes==155u*1024u);
  char name[16];IfsFsEntryV2 e={.structure_bytes=sizeof(e),.name=name,.name_capacity=sizeof(name)};
  assert(core->stat(v,"/00/proof.bin",&e)==0&&e.size_bytes==LENGTH&&e.type==IFS_FS_ENTRY_FILE);
  assert(core->directory_entry(v,"/00",0,&e)==0&&!strcmp(name,"PROOF.BIN"));
  assert(core->directory_entry(v,"/00",1,&e)==IFS_FS_END);
  assert(core->stat(v,"/03/NOTE.TXT",&e)==0&&e.size_bytes==3);
  assert(core->read(v,"/00/proof.bin",0,actual,LENGTH)==0&&!memcmp(proof,actual,LENGTH));
  assert(core->read(v,"/00/proof.bin",16001,actual,1002)==0&&!memcmp(proof+16001,actual,1002));
  assert(core->read(v,"/03/NOTE.TXT",0,actual,3)==0&&!memcmp(actual,"yes",3));
  assert(core->read(v,"/00/proof.bin",LENGTH,actual,1)==IFS_FS_INVALID);
  assert(core->unmount(v)==0&&!c.allocs);h.capabilities&=~IFS_FS_HOST_EXCLUSIVE;
  if(argc==2){
    c.fail=1;expect(core,&h,&c,IFS_FS_IO_ERROR);c.fail=0;
    image[BASE+32+16]=2;expect(core,&h,&c,IFS_FS_CORRUPT);
    fixture();image[BASE+32+16]=0;expect(core,&h,&c,IFS_FS_UNSUPPORTED);
    fixture();image[0x2b]=0;expect(core,&h,&c,IFS_FS_NOT_FOUND);
    fixture();image[128+4]=9;expect(core,&h,&c,IFS_FS_UNSUPPORTED);
  }
 }
 if(c.fd>=0)assert(!close(c.fd));
 assert(!dlclose(lib));puts("CPM native profile, exact file bytes, extent rollover and allocation refusal PASS");return 0;
}
