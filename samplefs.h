
#define SAMPLEFS_ROOT_I 2
/* samplefs mount flags */
#define SFS_MNT_CASE 1

/* This is an example of filesystem specific mount data that a file system might
   want to store.  FS per-superblock data varies widely and some fs do not
   require any information beyond the generic info which is already in
   struct super_block */
struct samplefs_sb_info {
//	int samplefs_var1;
//	int samplefs_var2;
	unsigned int rsize;
	unsigned int wsize;
	int flags;
	struct nls_table *local_nls;
};

static inline struct samplefs_sb_info *
SFS_SB(struct super_block *sb)
{
	return sb->s_fs_info;
}

