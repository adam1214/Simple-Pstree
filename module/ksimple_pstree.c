#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <net/sock.h>
#include <linux/netlink.h>

#include <linux/pid.h> //for finding struct task_struct
#include <linux/list.h> //for operating struct list_head

#define NETLINK_USER  22
#define USER_MSG    (NETLINK_USER + 1)
#define USER_PORT   50

MODULE_LICENSE("GPL");
MODULE_AUTHOR("arvik");
MODULE_DESCRIPTION("netlink_demo");

static struct sock *netlinkfd = NULL;

struct list_head* lptr=NULL;
struct task_struct* temp=NULL;
struct task_struct* target;
struct pid* pid_struct;

char send_data[1000000]= {'\0'};
char send_data_temp[100]= {'\0'};

int con=0;
int re_con=0;
int i=0;

void re_p(struct task_struct* target)
{
    if(target==NULL||target->pid==0) {
        printk("This target is NULL\n");
        return;
    }

    re_p(target->parent);

    char str[50]="";
    char space[100]= {'\0'};
    char s[5]="    "; //4 space
    for(i=0; i<re_con; i++) {
        strcat(space,s);
    }
    sprintf(str,"%s%s(%d)\n",space,target->comm,target->pid);
    strcat(send_data,str);
    re_con++;
}

void re_c(struct task_struct* target)
{
    struct list_head *ilptr=NULL;
    printk("c target %d\n", target->pid);
    if(target==NULL || target->pid==0) {
        printk("This target is NULL\n");
        return;
    }

    char *str=(char*)vmalloc(10000);
    char *space=(char*)vmalloc(9000);
    str[0] = space[0] = 0;
    char s[5]="    "; //4 space
    for(i=0; i<re_con; i++) {
        strcat(space,s);
    }
    sprintf(str,"%s%s(%d)\n",space,target->comm,target->pid);
    strcat(send_data,str);
    vfree(str);
    vfree(space);
    re_con++;
    if(!list_empty(&target->children))
        list_for_each(ilptr,&target->children) {
        temp=list_entry(ilptr,struct task_struct,sibling);
        re_c(temp);
    }
    re_con--;
}

static void str_clear(char a[],int len)
{
    int i;
    for(i=0; i<len; i++) {
        a[i]='\0';
    }
}

int send_msg(int8_t *pbuf, uint16_t len)
{
    struct sk_buff *nl_skb;
    struct nlmsghdr *nlh;
    int ret;
    nl_skb = nlmsg_new(len, GFP_ATOMIC);
    if(!nl_skb) {
        printk("netlink_alloc_skb error\n");
        return -1;
    }
    nlh = nlmsg_put(nl_skb, 0, 0, USER_MSG, len, 0);
    if(nlh == NULL) {
        printk("nlmsg_put() error\n");
        nlmsg_free(nl_skb);
        return -1;
    }
    memcpy(nlmsg_data(nlh), pbuf, len);
    ret = netlink_unicast(netlinkfd, nl_skb, USER_PORT, MSG_DONTWAIT);
    return ret;
}

static void recv_cb(struct sk_buff *skb)
{
    char *recv_ptr=NULL;
    char *type=NULL;
    char pid[100]= {'\0'};
    int pid_i=-1;

    struct nlmsghdr *nlh = NULL;
    void *data = NULL;

    char str[100]="";
    char arr_sibling[100]="";
    re_con=0;
    con=0;
    str_clear(send_data,strlen(send_data));
    str_clear(send_data_temp,strlen(send_data_temp));

    printk("skb->len:%u\n", skb->len);
    if(skb->len >= nlmsg_total_size(0)) {
        nlh = nlmsg_hdr(skb);
        data = NLMSG_DATA(nlh);
        if(data) {
            printk("kernel receive data: %s\n", (int8_t *)data);

            recv_ptr=(char*)data; //convert to char type
            printk("convert to char type successfully");

            type=recv_ptr[0]; //c or s or p
            printk("type = %c\n",type);

            for(i=0; i<(strlen(recv_ptr)-1); i++) { //make pid
                if(i==(strlen(recv_ptr)-2)) {
                    pid[i]='\0';
                } else {
                    pid[i]=recv_ptr[i+2];
                }
            }

            printk("pid = %s\n",pid);
            if(pid_i<0) {
                sscanf(pid,"%d",&pid_i);
            }
            printk("pid_i = %d.\n",pid_i);

            pid_struct = find_get_pid(pid_i);
            target=pid_task(pid_struct,PIDTYPE_PID);
            printk("target(task_struct) be made successfully");
            if(target!=NULL) {
                printk("target:%s(%d)\n",target->comm,target->pid);
            }

            if(type=='s') {
                if(target!=NULL) {
                    list_for_each(lptr,&target->parent->children) { //travel to each list_head
                        printk("Into for loop\n");
                        temp=list_entry(lptr,struct task_struct,sibling);
                        printk("temp(task_struct) be made successfully\n");
                        if(target->pid!=temp->pid) { //avoid to print target process
                            sprintf(arr_sibling,"%s(%d)\n",temp->comm,temp->pid);
                            strcat(send_data,arr_sibling);
                        }
                    }
                    send_data[strlen(send_data)]='\0';
                    send_msg(send_data,strlen(send_data));
                } else {
                    send_msg(send_data,strlen(send_data));
                }
            }

            else if(type=='c') {
                if(target!=NULL) {
                    re_c(target);
                    printk("re_c done\n");
                    printk("re_con=%d\n",re_con);
                    send_msg(send_data,strlen(send_data));
                } else {
                    send_msg(send_data,strlen(send_data));
                }
            } else if(type=='p') {
                if(target!=NULL) {
                    re_p(target);
                    send_msg(send_data,strlen(send_data));
                } else {
                    send_msg(send_data,strlen(send_data));
                }
            }

            //send_msg(data, nlmsg_len(nlh));
        }
        /*if(strcmp( (int8_t *)data , "-c") == 0)
        {
        	printk("kernel receive data: %s\n",(int8_t *)data);
        	send_msg(data, nlmsg_len(nlh));
        }*/

    }
}

struct netlink_kernel_cfg cfg = {
    .input = recv_cb, //cfg pointer points to receieve function
};

static int __init test_netlink_init(void)
{
    printk("init netlink_demo!\n");
    netlinkfd = netlink_kernel_create(&init_net, USER_MSG, &cfg);
    if(!netlinkfd) {
        printk(KERN_ERR "can not create a netlink socket!\n");
        return -1;
    }
    printk("netlink demo init ok!\n");
    return 0;
}

static void __exit test_netlink_exit(void)
{
    sock_release(netlinkfd->sk_socket);
    printk(KERN_DEBUG "netlink exit\n!");
}

module_init(test_netlink_init);
module_exit(test_netlink_exit);
