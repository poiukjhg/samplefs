#include <linux/ctype.h>
#include <linux/slab.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/pagemap.h>
#include <linux/version.h>
#include <linux/nls.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/backing-dev.h>
#include <linux/seq_file.h>
#include "samplefs.h"

/* helpful if this is different than other fs */
#define SAMPLEFS_MAGIC     0x73616d70 /* "SAMP" */
#define CONFIG_SAMPLEFS_DEBUG
#ifndef CONFIG_PROC_FS
#define CONFIG_PROC_FS 1
#endif

unsigned int sample_parm = 0;
module_param(sample_parm, int, 0);
MODULE_PARM_DESC(sample_parm, "An example parm. Default: x Range: y to z");

extern struct file_operations sfs_file_operations;
extern struct address_space_operations sfs_aops;
extern struct inode_operations sfs_dir_inode_ops;
extern struct inode_operations sfs_file_inode_ops;

static void samplefs_put_super(struct super_block * sb)
{
	struct samplefs_sb_info *sfs_sb;
	sfs_sb = SFS_SB(sb);
	if (sfs_sb == NULL){
		return;
	}
	unload_nls(sfs_sb->local_nls);
	kfree(sfs_sb);
	return;
}

struct super_operations samplefs_super_ops = {
	.statfs = simple_statfs,
	.drop_inode = generic_delete_inode,
	.put_super = samplefs_put_super,
};

static void
samplefs_parse_mount_options(char *options, struct samplefs_sb_info *sfs_sb)
{
	char *value;
	char *data;
	int size;

	if (!options)
		return;

	while((data = strsep(&options, ",")) != NULL){
		if (!*data)
			continue;
		if ((value = strchr(data, '=')) != NULL)
			*value++ = '\0';
		if (strnicmp(data, "rsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0)
					sfs_sb->rsize = size;
					printk(KERN_INFO
						"samplefs: rsize %d\n", size);
			}
		} else if (strnicmp(data, "wsize", 5) == 0) {
			if (value && *value) {
				size = simple_strtoul(value, &value, 0);
				if (size > 0){
					sfs_sb->wsize = size;
					printk(KERN_INFO
						"samplefs: wsize %d\n", size);
				}				
			}	
		} else if ((strnicmp(data, "nocase", 6) == 0) ||
			   (strnicmp(data, "ignorecase", 10)  == 0)) {
			sfs_sb->flags |= SFS_MNT_CASE;
			printk(KERN_INFO "samplefs: ignore case\n");

		} else {
			printk(KERN_WARNING "samplefs: bad mount option %s\n",
				data);
		}

	}


}

static int sfs_ci_hash(const struct dentry *dentry, struct qstr *q)
{
	struct nls_table *codepage =SFS_SB(dentry->d_inode->i_sb)->local_nls;
	unsigned long hash;
	int i;
	hash = init_name_hash();

	for(i = 0; i< q->len; i++){
		hash = partial_name_hash(nls_tolower(codepage, q->name[i]),
			hash);		
	}
	q->hash = end_name_hash(hash);
	return 0;
}

static int sfs_ci_compare(const struct dentry *parent, const struct dentry *dentry,
        unsigned int len, const char *str, const struct qstr *name)
{
    int i, result = 1; 

    if (len != name->len)
        goto out; 
    for (i=0; i < len; i++) {
        if (tolower(str[i]) != tolower(name->name[i]))
            goto out; 
    }    
    result = 0; 
out:
    return result;
}

/* No sense hanging on to negative dentries as they are only
in memory - we are not saving anything as we would for network
or disk filesystem */
static int sfs_delete_dentry(const struct dentry *dentry)
{
        return 1;
}

struct dentry_operations sfs_dentry_ops = {
	.d_delete = sfs_delete_dentry,
};

static struct dentry_operations sfs_ci_dentry_ops = {
/*	.d_revalidate = xxxd_revalidate, Not needed for this type of fs */	
	.d_hash = sfs_ci_hash,
	.d_compare = sfs_ci_compare,
	.d_delete = sfs_delete_dentry,
};

static struct backing_dev_info sfs_backing_dev_info = {
	.ra_pages       = 0,    /* No readahead */
	.capabilities   = BDI_CAP_NO_ACCT_DIRTY | BDI_CAP_NO_WRITEBACK |
			  BDI_CAP_MAP_DIRECT | BDI_CAP_MAP_COPY |
			  BDI_CAP_READ_MAP | BDI_CAP_WRITE_MAP |
			  BDI_CAP_EXEC_MAP,
};

/*
 * Lookup the data, if the dentry didn't already exist, it must be
 * negative.  Set d_op to delete negative dentries to save memory
 * (and since it does not help performance for in memory filesystem).
 */
struct inode *samplefs_get_inode(struct super_block * sb, int mode, dev_t dev)
{
	struct inode *inode = new_inode(sb);
	struct samplefs_sb_info * sfs_sb = SFS_SB(sb);
	if (inode){
		inode->i_mode = mode;
		inode->i_uid = current_fsuid();
		inode->i_gid = current_fsgid();
		inode->i_blocks = 0;
		inode->i_atime = inode->i_mtime =  inode->i_ctime  = CURRENT_TIME;
		printk(KERN_INFO "about to set inode ops\n");
		inode->i_mapping->a_ops = &sfs_aops;
		inode->i_mapping->backing_dev_info = &sfs_backing_dev_info;
		switch(mode & S_IFMT){			
			case S_IFDIR:
				printk(KERN_INFO "directory inode sfs_sb: %p\n",sfs_sb);
				inode->i_op = &sfs_dir_inode_ops;				
				inode->i_fop = &simple_dir_operations;				
				inc_nlink(inode);				
				break;
			case S_IFREG:
				printk(KERN_INFO "file inode\n");
				inode->i_op = &sfs_file_inode_ops;
				inode->i_fop =  &sfs_file_operations;
				break;
			case S_IFLNK:
				inode->i_op = &page_symlink_inode_operations;
				break;				
			default:
				init_special_inode(inode, mode, dev);
			break;
			
		}
	}
	return inode;
}
static int samplefs_fill_super(struct super_block * sb, void * data, int silent)
{
	struct inode *inode;
	struct samplefs_sb_info *sfs_sb;

	sb->s_maxbytes = MAX_LFS_FILESIZE;
	sb->s_blocksize = PAGE_CACHE_SIZE;
	sb->s_blocksize_bits = PAGE_CACHE_SHIFT;
	sb->s_magic = SAMPLEFS_MAGIC;
	sb->s_op = &samplefs_super_ops;
	sb->s_time_gran = 1;
	
	printk(KERN_INFO "samplefs: fill super\n");
/*	
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 4, 25)
	     inode = iget_locked(sb, SAMPLEFS_ROOT_I);
	#else
	  	 inode = iget(sb, SAMPLEFS_ROOT_I);
	#endif	
*/
	inode = samplefs_get_inode(sb, S_IFDIR | 0755, 0);
	if ( !inode )
		return -ENOMEM;
#ifdef CONFIG_SAMPLEFS_DEBUG
	printk(KERN_INFO "samplefs: about to alloc s_fs_info\n");
#endif
	sb->s_fs_info = kzalloc(sizeof(struct samplefs_sb_info), GFP_KERNEL);
	sfs_sb = SFS_SB(sb);
	if (!sfs_sb){
		iput(inode);
		return -ENOMEM;
	}
	printk(KERN_INFO "samplefs: about to alloc dentry root inode\n");
	#if LINUX_VERSION_CODE >= KERNEL_VERSION(3, 4, 0)
	sb->s_root = d_make_root(inode);
       #else
       sb->s_root  = d_alloc_root(inode);
       #endif	
	if (!sb->s_root)	{
		iput(inode);
		kfree(sfs_sb);
		sb->s_fs_info = NULL;
		return -ENOMEM;
	}
	
	sfs_sb->local_nls = load_nls_default();
	samplefs_parse_mount_options(data, sfs_sb);
	
	return 0;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,18)
struct super_block * samplefs_get_sb(struct file_system_type *fs_type,
        int flags, const char *dev_name, void *data)
{
	return get_sb_nodev(fs_type, flags, data, samplefs_fill_super);
}

#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
int samplefs_get_sb(struct file_system_type *fs_type,
	int flags, const char *dev_name, void *data, struct vfsmount *mnt)
{
	return get_sb_nodev(fs_type, flags, data, samplefs_fill_super, mnt);
}

#else
struct dentry * samplefs_get_sb(struct file_system_type *fs_type,
int flags, const char *dev_name, void *data)
{	
	return mount_nodev(fs_type, flags, data, samplefs_fill_super);
}
#endif


static struct file_system_type samplefs_fs_type = {
	.owner = THIS_MODULE,
	.name = "samplefs",
	.mount = samplefs_get_sb,
	.kill_sb = kill_litter_super,
	/*  .fs_flags */
};

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *proc_fs_samplefs;
/*
static int 
sfs_debug_read(char *buf, char **beginBuffer, off_t offset,
		int count, int *eof, void *data)
{
	int length = 0;
	char *original_buf = buf;
	*beginBuffer = buf+offset;
	length = sprintf(buf,
		"Display Debugging Information\n"
		"-----------------------------\n");
	buf +=length;

	length = buf - original_buf;
	if (offset +count >= length)
		*eof = 1;
	if (length < offset){
		*eof = 1;
		return 0;
	}else{
		length = length - offset;
	}
	if (length > count)
		length = count;
	return length;
}
*/
static struct proc_dir_entry *proc_entry;
static const struct file_operations proc_file_fops = {
 .owner = THIS_MODULE,
 .read = seq_read,
};

void
sfs_proc_init(void)
{
	
	proc_fs_samplefs =proc_mkdir("samplefs", NULL);
	if(proc_fs_samplefs == NULL)
		return;
	proc_entry = proc_create( "DebugData", 0, proc_fs_samplefs, &proc_file_fops );	
	if (proc_entry == NULL) {
		printk(KERN_INFO "fortune: Couldn't create proc entry\n");
		return;
	} 
	//proc_fs_samplefs->owner = THIS_MODULE;	
	//	create_proc_read_entry("DebugData", 0, proc_fs_samplefs,
	//	sfs_debug_read, NULL);
}
void
sfs_proc_clean(void)
{
	if(proc_fs_samplefs == NULL)
		return;
	remove_proc_entry("DebugData", proc_fs_samplefs);
	remove_proc_entry("samplefs", NULL);
}
#endif
static int __init init_samplefs_fs(void)
{
	printk(KERN_INFO "init samplefs\n");
#ifdef CONFIG_PROC_FS
	sfs_proc_init();
#endif
	if (sample_parm>256) {
		printk(KERN_ERR "sample_parm %d too large, reset to 10\n",
			sample_parm);
		sample_parm = 10;
	}
	return register_filesystem(&samplefs_fs_type);
}

static void __exit exit_samplefs_fs(void)
{
	printk(KERN_INFO "unloading samplefs\n");
#ifdef CONFIG_PROC_FS
	sfs_proc_clean();
#endif
	unregister_filesystem(&samplefs_fs_type);
}

MODULE_LICENSE("GPL");
module_init(init_samplefs_fs);
module_exit(exit_samplefs_fs);


