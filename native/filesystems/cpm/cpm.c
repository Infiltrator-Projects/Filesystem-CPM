/* SPDX-License-Identifier: GPL-3.0-or-later
 * Native CP/M Plus reader for the explicitly identified Amstrad/JCE
 * PCW180 linear disk profile. Format reference: retained cpmtools
 * amsReadSuper and cpmdir.h, e534e20c15973a9559e981efb498a102020e5db7.
 * No guessed raw CP/M geometry, OS filesystem, library or process proxy.
 */
#include <infiltratr/filesystem_module_v2.h>
#define IMAGE_BYTES 184320u
#define BASE 4608u
#define BLOCK 1024u
#define BLOCKS 175u
#define DIRECTORY_BLOCKS 2u
#define SLOTS 64u
#define EXTENTS 11u
#define EXTENT_BYTES 16384u
#define PROFILE 1u

typedef struct {
    uint8_t user, slots[EXTENTS]; /* directory slot + 1; zero is absent */
    uint32_t size;
    char name[13];
} File;
typedef struct {
    const IfsFsHostRuntimeV2 *host;
    uint8_t dir[SLOTS*32u], allocated[BLOCKS];
    File files[SLOTS];
    uint32_t count, used;
} Volume;
static void zero(void *p,size_t n){uint8_t *b=p;while(n--)*b++=0;}
static int equal(const uint8_t *p,const uint8_t *q,uint32_t n)
{while(n--)if(*p++!=*q++)return 0;return 1;}
static int same(const char *a,const char *b)
{while(*a&&*a==*b){a++;b++;}return *a==*b;}
static char upper(char c){return c>='a'&&c<='z'?(char)(c-'a'+'A'):c;}
static int valid_char(unsigned c)
{
    /* A bounded ASCII namespace. Reject rather than alias unqualified
     * control characters, path separators and CP/M wildcard syntax. */
    return c>=33u&&c<=126u&&c!='<'&&c!='>'&&c!='.'&&c!=','&&c!=';'&&
        c!=':'&&c!='='&&c!='?'&&c!='*'&&c!='['&&c!=']'&&c!='/'&&c!='\\';
}
static int native_name(const uint8_t *d,char *name)
{
    unsigned n=0;
    for(unsigned field=0;field<2;field++){
        unsigned width=field?3u:8u,start=field?9u:1u,used=width;
        while(used&&(d[start+used-1u]&127u)==' ')used--;
        if(!field&&!used)return IFS_FS_CORRUPT;
        if(field&&used)name[n++]='.';
        for(unsigned i=0;i<used;i++){
            unsigned c=d[start+i]&127u;
            if(!valid_char(c))return IFS_FS_UNSUPPORTED;
            name[n++]=upper((char)c);
        }
    }
    name[n]=0;return IFS_FS_OK;
}
static int geometry(const IfsFsHostRuntimeV2 *h)
{
    uint8_t b[512];
    static const uint8_t spec[8]={0,0,40,9,2,1,3,2};
    if(!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    if(h->volume_bytes!=IMAGE_BYTES)return IFS_FS_NOT_FOUND;
    int rc=ifs_fs_host_read_bytes_v2(h,0,b,sizeof(b));if(rc)return rc;
    /* CP/M has no universal signature. Require the documented JCE
     * CP/M/DSK extension, which explicitly identifies the boot spec at
     * byte 128. A plausible unmarked raw directory is not sufficient. */
    if((b[0]!=0xe9u&&b[0]!=0xebu)||
       !equal(b+0x2b,(const uint8_t*)"CP/M",4)||
       !equal(b+0x33,(const uint8_t*)"DSK",3)||
       !equal(b+0x7c,(const uint8_t*)"CP/M",4))return IFS_FS_NOT_FOUND;
    return equal(b+128,spec,sizeof(spec))?IFS_FS_OK:IFS_FS_UNSUPPORTED;
}
static uint32_t record_size(const uint8_t *d)
{
    /* CP/M Plus uses RC records and an optional exact last-record byte
     * count. LRC zero denotes a complete final 128-byte record. */
    return d[15]?((uint32_t)d[15]-1u)*128u+(d[13]?d[13]:128u):0u;
}
static int decode(Volume *v)
{
    int rc=ifs_fs_host_read_bytes_v2(v->host,BASE,v->dir,sizeof(v->dir));
    if(rc)return rc;
    v->allocated[0]=v->allocated[1]=1;v->used=DIRECTORY_BLOCKS;
    for(unsigned slot=0;slot<SLOTS;slot++){
        const uint8_t *d=v->dir+slot*32u;
        if(d[0]==0xe5u||d[0]==0x20u||d[0]==0x21u)continue;
        /* Password records and high user IDs have different semantics;
         * refusing the volume avoids exposing password-protected files. */
        if(d[0]>15u)return IFS_FS_UNSUPPORTED;
        if((d[12]&0xe0u)||(d[14]&0xc0u)||d[15]>128u||d[13]>127u||
           (!d[15]&&d[13]))return IFS_FS_CORRUPT;
        unsigned ext=(d[12]&31u)|((d[14]&63u)<<5);
        if(ext>=EXTENTS)return IFS_FS_CORRUPT;
        char name[13];rc=native_name(d,name);if(rc)return rc;
        unsigned f;
        for(f=0;f<v->count;f++)if(v->files[f].user==d[0]&&same(v->files[f].name,name))break;
        if(f==v->count){
            if(v->count==SLOTS)return IFS_FS_CORRUPT;
            v->count++;v->files[f].user=d[0];
            unsigned i=0;do{v->files[f].name[i]=name[i];}while(name[i++]);
        }
        File *file=&v->files[f];
        if(file->slots[ext])return IFS_FS_CORRUPT;
        file->slots[ext]=(uint8_t)(slot+1u);
        uint32_t bytes=record_size(d),needed=(bytes+BLOCK-1u)/BLOCK;
        for(unsigned k=0;k<16u;k++){
            unsigned block=d[16+k];
            if(!block){if(k<needed)return IFS_FS_UNSUPPORTED;continue;}
            if(k>=needed)return IFS_FS_UNSUPPORTED;
            if(block<DIRECTORY_BLOCKS||block>=BLOCKS||v->allocated[block])return IFS_FS_CORRUPT;
            v->allocated[block]=1;v->used++;
        }
        uint32_t end=ext*EXTENT_BYTES+bytes;
        if(end>file->size)file->size=end;
    }
    /* Restrict this first reader to ordinary dense logical files.
     * Physical fragmentation is supported; sparse/random-record holes
     * are deliberately not silently synthesised. */
    for(unsigned f=0;f<v->count;f++){
        File *file=&v->files[f];unsigned last=0;
        for(unsigned e=0;e<EXTENTS;e++)if(file->slots[e])last=e;
        for(unsigned e=0;e<=last;e++){
            if(!file->slots[e])return IFS_FS_UNSUPPORTED;
            const uint8_t *d=v->dir+(file->slots[e]-1u)*32u;
            if(e<last&&record_size(d)!=EXTENT_BYTES)return IFS_FS_UNSUPPORTED;
            if(e==last&&last&&!record_size(d))return IFS_FS_UNSUPPORTED;
        }
    }
    return IFS_FS_OK;
}
static int load(const IfsFsHostRuntimeV2 *h,Volume **out)
{
    *out=NULL;
    int rc=geometry(h);if(rc)return rc;
    Volume *v=h->allocate(h->context,sizeof(*v));if(!v)return IFS_FS_NO_MEMORY;
    zero(v,sizeof(*v));v->host=h;
    rc=decode(v);
    if(rc){h->release(h->context,v);return rc;}
    *out=v;return IFS_FS_OK;
}
static int probe(const IfsFsHostRuntimeV2 *h,uint32_t *variant)
{
    if(!variant)return IFS_FS_INVALID;
    *variant=0;Volume *v;int rc=load(h,&v);
    if(!rc){h->release(h->context,v);*variant=PROFILE;}
    return rc;
}
static int query_volume(void *volume,IfsFsVolumeInfoV2 *info)
{
    Volume *v=volume;if(!v||!info||info->structure_bytes<sizeof(*info))return IFS_FS_INVALID;
    info->variant=PROFILE;info->max_name_bytes=12;info->block_bytes=BLOCK;
    info->total_bytes=IMAGE_BYTES;info->free_bytes=(BLOCKS-v->used)*BLOCK;
    return IFS_FS_OK;
}
static int mount_volume(const IfsFsHostRuntimeV2 *h,void **out,IfsFsVolumeInfoV2 *info)
{
    if(!out||!info||info->structure_bytes<sizeof(*info)||!ifs_fs_host_v2_valid(h))return IFS_FS_INVALID;
    *out=NULL;
    if((h->capabilities&(IFS_FS_HOST_READ_ONLY|IFS_FS_HOST_EXCLUSIVE))!=(IFS_FS_HOST_READ_ONLY|IFS_FS_HOST_EXCLUSIVE))return IFS_FS_ACCESS;
    Volume *v;int rc=load(h,&v);if(rc)return rc;
    *out=v;return query_volume(v,info);
}
static int unmount_volume(void *volume)
{
    Volume *v=volume;if(!v)return IFS_FS_INVALID;
    v->host->release(v->host->context,v);return IFS_FS_OK;
}
static int resolve(Volume *v,const char *path,unsigned *user,File **file)
{
    *file=NULL;*user=16;
    if(!v||!path||path[0]!='/')return IFS_FS_INVALID;
    if(!path[1])return IFS_FS_OK;
    if(path[1]<'0'||path[1]>'1'||path[2]<'0'||path[2]>'9')return IFS_FS_NOT_FOUND;
    *user=(unsigned)(path[1]-'0')*10u+(unsigned)(path[2]-'0');
    if(*user>=16u)return IFS_FS_NOT_FOUND;
    if(!path[3])return IFS_FS_OK;
    if(path[3]!='/')return IFS_FS_NOT_FOUND;
    char name[13];unsigned n=0;
    for(const char *p=path+4;*p;p++){
        if(n>=12u||*p=='/')return IFS_FS_INVALID;
        name[n++]=upper(*p);
    }
    if(!n)return IFS_FS_INVALID;
    name[n]=0;
    for(unsigned i=0;i<v->count;i++)
        if(v->files[i].user==*user&&same(v->files[i].name,name)){*file=&v->files[i];return IFS_FS_OK;}
    return IFS_FS_NOT_FOUND;
}
static int entry(IfsFsEntryV2 *e,const char *name,uint32_t type,uint64_t size)
{
    if(!e||e->structure_bytes<sizeof(*e)||!e->name)return IFS_FS_INVALID;
    unsigned n=0;while(name[n])n++;
    if(n>=e->name_capacity)return IFS_FS_INVALID;
    for(unsigned i=0;i<=n;i++)e->name[i]=name[i];
    e->name_bytes=n;e->type=type;e->size_bytes=size;e->attributes=0;return IFS_FS_OK;
}
static void area(unsigned user,char name[3]){name[0]=(char)('0'+user/10);name[1]=(char)('0'+user%10);name[2]=0;}
static int stat_path(void *volume,const char *path,IfsFsEntryV2 *e)
{
    unsigned user;File *f;int rc=resolve(volume,path,&user,&f);if(rc)return rc;
    if(f)return entry(e,f->name,IFS_FS_ENTRY_FILE,f->size);
    if(user==16u)return entry(e,"/",IFS_FS_ENTRY_DIRECTORY,0);
    char name[3];area(user,name);return entry(e,name,IFS_FS_ENTRY_DIRECTORY,0);
}
static int list_path(void *volume,const char *path,uint32_t index,IfsFsEntryV2 *e)
{
    Volume *v=volume;unsigned user;File *f;int rc=resolve(v,path,&user,&f);if(rc)return rc;
    if(f)return IFS_FS_INVALID;
    /* The 16 logical user areas are an interface namespace, not on-disk
     * hierarchical directories or invented file records. */
    if(user==16u){
        if(index>=16u)return IFS_FS_END;
        char name[3];area(index,name);return entry(e,name,IFS_FS_ENTRY_DIRECTORY,0);
    }
    for(unsigned i=0;i<v->count;i++)if(v->files[i].user==user){
        if(!index)return entry(e,v->files[i].name,IFS_FS_ENTRY_FILE,v->files[i].size);
        index--;
    }
    return IFS_FS_END;
}
static int read_path(void *volume,const char *path,uint64_t offset,void *destination,uint32_t length)
{
    Volume *v=volume;unsigned user;File *f;int rc=resolve(v,path,&user,&f);if(rc)return rc;
    if(!f||(!destination&&length)||offset>f->size||length>f->size-offset)return IFS_FS_INVALID;
    uint8_t *out=destination;
    while(length){
        unsigned extent=(unsigned)(offset/EXTENT_BYTES),slot=f->slots[extent];
        if(!slot)return IFS_FS_CORRUPT;
        unsigned within=(unsigned)(offset%EXTENT_BYTES),skip=within%BLOCK;
        unsigned physical=v->dir[(slot-1u)*32u+16u+within/BLOCK],n=BLOCK-skip;
        if(!physical||physical>=BLOCKS)return IFS_FS_CORRUPT;
        if(n>length)n=length;
        rc=ifs_fs_host_read_bytes_v2(v->host,BASE+(uint64_t)physical*BLOCK+skip,out,n);
        if(rc)return rc;
        offset+=n;out+=n;length-=n;
    }
    return IFS_FS_OK;
}
static const IfsFsCoreOpsV2 core={
 .header={2,sizeof(IfsFsCoreOpsV2),IFS_FS_GROUP_CORE,0,IFS_FS_CORE_PROBE|IFS_FS_CORE_MOUNT|IFS_FS_CORE_STAT|IFS_FS_CORE_LIST|IFS_FS_CORE_READ},
 .probe=probe,.mount=mount_volume,.unmount=unmount_volume,.stat=stat_path,.directory_entry=list_path,.read=read_path
};
static const IfsFsVolumeOpsV2 volume_ops={
 .header={2,sizeof(IfsFsVolumeOpsV2),IFS_FS_GROUP_VOLUME,0,IFS_FS_VOLUME_QUERY},.query=query_volume
};
static const IfsFsGroupHeaderV2 *group(uint32_t id,uint32_t min,uint32_t max)
{
    if(min>2u||max<2u)return NULL;
    if(id==IFS_FS_GROUP_CORE)return &core.header;
    return id==IFS_FS_GROUP_VOLUME?&volume_ops.header:NULL;
}
static const IfsFsModuleApiV2 api={
 .abi_version=2,.structure_bytes=sizeof(IfsFsModuleApiV2),.format_id="cpm",
#ifdef IFS_FREESTANDING
 .runtime_profiles=IFS_FS_PROFILE_FREESTANDING,
#else
 .runtime_profiles=IFS_FS_PROFILE_HOSTED,
#endif
 .group_mask=IFS_FS_GROUP_BIT(IFS_FS_GROUP_CORE)|IFS_FS_GROUP_BIT(IFS_FS_GROUP_VOLUME),.query_group=group
};
__attribute__((visibility("default"))) const IfsFsModuleApiV2 *infiltrator_filesystem_api_v2(void){return &api;}
