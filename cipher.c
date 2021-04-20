#include <linux/init.h>
#include <linux/module.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/ioctl.h>
#define MAX_DEV 2
unsigned char c[4096];
unsigned char key[128];
unsigned char message[4096];
bool samekey;
static struct proc_dir_entry *key_entry;
static struct proc_dir_entry *msg_entry;

struct mychar_device_data {
    struct cdev cdev;
    char __user buffer;
    size_t size;
};

void rc4(unsigned char * p, unsigned char * k, unsigned char * c,int l)
{
        unsigned char s [256];
        unsigned char t [256];
        unsigned char temp;
        unsigned char kk;
        int i,j,x;
        for ( i  = 0 ; i  < 256 ; i ++ )
        {
                s[i] = i;
                t[i]= k[i % 4];
        }
        j = 0 ;
        for ( i  = 0 ; i  < 256 ; i ++ )
        {
                j = (j+s[i]+t[i])%256;
                temp = s[i];
                s[i] = s[j];
                s[j] = temp;
        }

        i = j = -1;
        for ( x = 0 ; x < l ; x++ )
        {
                i = (i+1) % 256;
                j = (j+s[i]) % 256;
                temp = s[i];
                s[i] = s[j];
                s[j] = temp;
                kk = (s[i]+s[j]) % 256;
                c[x] = p[x] ^ s[kk];
        }
}


static int my_open(struct inode *inode, struct file *file)
{
    struct mychar_device_data *mydata;
    printk("Device opened\n");
    mydata= container_of(inode->i_cdev,struct mychar_device_data,cdev);
    file->private_data =mydata;
    return 0;
}

static int my_release(struct inode *inode, struct file *file)
{
    printk("Device closed\n");
    return 0;
}



static ssize_t my_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
    int minor;
 
    minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
    if(minor==1){
	    printk("Go away you can't see my key\n");
	return 0;
    }
    else{

   uint8_t *data = "Hello from the kernel world!\n";
    size_t datalen = strlen(data);

    printk("Reading device: %d\n", minor);
	printk(c);
  if (count > datalen) {
        count = datalen;
	return 0;
    }

    if (copy_to_user(buf, data, count)) {
        return -EFAULT;
    }
    }

    return count;
}

static ssize_t my_write(struct file *file, const char __user *buf, size_t count, loff_t *offset)
{	
  
    int minor;
  size_t maxdatalen = 128, ncopied;
    uint8_t keybuf[maxdatalen];
   size_t maxmsglen = 4096, ncop;
    uint8_t msgbuf[maxmsglen];
      
 minor = MINOR(file->f_path.dentry->d_inode->i_rdev);
    if(minor==1){	    
    printk("Writing device: %d\n",minor);
    if (count < maxdatalen) {
        maxdatalen = count;
    }
    ncopied = copy_from_user(keybuf, buf, maxdatalen);
    keybuf[maxdatalen] = 0;
    printk("Data from the user: %s\n", keybuf);
    memcpy(key,keybuf,sizeof(keybuf));
    key[maxdatalen]=0;
  /* printk(key);*/
    } 
   
    else{
	  printk("Writing device: %d\n", minor);
    	if (count < maxmsglen) {
        	maxmsglen = count;
	    }
	    ncop = copy_from_user(msgbuf, buf, maxmsglen);
	    msgbuf[maxmsglen] = 0;
	    printk("Data from the user: %s\n", msgbuf);
	    memcpy(message,msgbuf,sizeof(msgbuf));
	    message[maxmsglen]=0;
	rc4(msgbuf,keybuf,c,1000);
	
    }

    return count;
}

static const struct file_operations mychardev_fops = {
    .owner      = THIS_MODULE,
    .open       = my_open,
    .release    = my_release,
    .read       = my_read,
    .write       = my_write
};


static ssize_t keywrite(struct file *file,const char __user *userbuf, size_t count,loff_t *pos){

	char temp[128];
	memset(temp,0,128);
	printk(KERN_DEBUG "write\n");
	  if (count > 128) {
        
	count = 128;
    }

    if (copy_from_user(temp, userbuf, count)) {
        return -EFAULT;
   	 }
    	
if(strcmp(temp,key)==0){
	samekey=true;
}
else{
	samekey=false;
}
	
	return count;

}

static ssize_t read(struct file *file,char __user *userbuf, size_t count,loff_t *pos){

	printk(KERN_DEBUG "read\n");
	if(samekey==false){

		printk("Nothing is here !\n");}
	else{
	printk(message);
	}
	return 0;

}

static int display(struct seq_file *s,void *v){

	seq_printf(s,"opened file\n");
}

static int open(struct inode *inode,struct file *f){

	return single_open(f,display,NULL);

}


static struct file_operations pfs =
{
	/*.read=read,*/
	.write= keywrite,
	.open=open,
	.llseek = seq_lseek,
};


static struct file_operations c_pfs =
{
	.read=read,
	/*.write= keywrite,*/
	.open=open,
	.llseek = seq_lseek,
};


static int major = 0;
static struct class *mychardev_class = NULL;
static struct mychar_device_data mychardev_data[MAX_DEV];

static int mychardev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}


static int __init dev_init(void)
{
    int err, i;
    dev_t dev;

    err = alloc_chrdev_region(&dev, 0, MAX_DEV, "mychardev");
    if(err !=0){
	printk("error !");
	return err;
    }

    major = MAJOR(dev);

    mychardev_class = class_create(THIS_MODULE, "mychardev");
    mychardev_class->dev_uevent = mychardev_uevent;

    for (i = 0; i < MAX_DEV; i++) {
        cdev_init(&mychardev_data[i].cdev, &mychardev_fops);
        mychardev_data[i].cdev.owner = THIS_MODULE;

        cdev_add(&mychardev_data[i].cdev, MKDEV(major, i), 1);

	if(i==0){
        device_create(mychardev_class, NULL, MKDEV(major, i), NULL, "cipher");
	}
	else{
	device_create(mychardev_class, NULL, MKDEV(major, i), NULL, "cipher_key");
	}
	
    }

   /* key_entry =proc_create("cipher_key",0660,NULL,&pfs);*/
  /*  msg_entry=proc_create("cipher",0660,NULL,&c_pfs);*/
    return 0;
}

static void __exit dev_exit(void)
{
    int i;

    for (i = 0; i < MAX_DEV; i++) {
        device_destroy(mychardev_class, MKDEV(major, i));
	cdev_del(&mychardev_data[i].cdev);
    }

    class_unregister(mychardev_class);
    class_destroy(mychardev_class);

    unregister_chrdev_region(MKDEV(major, 0), MAX_DEV);
   /*    proc_remove(key_entry);*/
   /* proc_remove(msg_entry);*/
}



MODULE_LICENSE("GPL");

module_init(dev_init);
module_exit(dev_exit);
