#include <linux/module.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/sched.h>
#include <linux/pid.h>
#include <linux/signal.h>
#include <linux/timer.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/ktime.h>

#define DEVICE_NAME "mydevice"
#define IOCTL_SET_PID _IOW('a', 'b', pid_t)
#define IOCTL_TRIGGER_APP _IO('a', 'c')
#define IOCTL_SIGNAL_RECEIVED _IO('a', 'd')
#define IOCTL_SIGNAL_CLOSE _IO('a', 'e')

#define bins 10

static pid_t user_pid = -1;
static int device_open_count = 0;
static int major;
static struct class *cls;

struct timer_list timer_;
unsigned long timer_duration = 1000;
ktime_t start_time;

int time_value_count = 0;
s64 sum_time = 0;
s64 max_time = 0;
s64 min_time = 0;


s32 history_time[10000];
int hist[bins];

void timer_callback(struct timer_list *t) {
    if (user_pid != -1) {
        struct pid *pid_struct = find_get_pid(user_pid);
        if (pid_struct) {
            struct task_struct *task = pid_task(pid_struct, PIDTYPE_PID);
            if (task) {
                send_sig(SIGUSR1, task, 0);
                start_time = ktime_get_real();
            } else {
                pr_alert("Invalid task for PID: %d\n", user_pid);
            }
            put_pid(pid_struct);
        } else {
            pr_alert("Invalid PID: %d\n", user_pid);
        }
    }
    mod_timer(&timer_, jiffies + msecs_to_jiffies(timer_duration));
}

static int device_open(struct inode *inode, struct file *file) {
    if (device_open_count)
        return -EBUSY;
    device_open_count++;
    pr_alert("Device open\n");
    return 0;
}

static int device_release(struct inode *inode, struct file *file) {
    if (device_open_count <= 0)
        return -EBUSY;
    device_open_count--;
    user_pid = -1;
    pr_alert("Device close\n");
    return 0;
}

static long device_ioctl(struct file *file, unsigned int cmd, unsigned long arg) {
    switch (cmd) {
        case IOCTL_SET_PID:
            copy_from_user(&user_pid, (pid_t *)arg, sizeof(pid_t));
            break;
        case IOCTL_SIGNAL_RECEIVED:
            {
                ktime_t end_time = ktime_get_real(); // Получаем время получения сигнала
                s64 elapsed_time = ktime_to_ns(ktime_sub(end_time, start_time)); // Вычисляем разницу
                sum_time += elapsed_time;
                history_time[time_value_count] = elapsed_time;

                
                if(elapsed_time > max_time)
                    max_time = elapsed_time;

                if(time_value_count == 0)
                    min_time = elapsed_time;

                if(elapsed_time < min_time)
                    min_time = elapsed_time;

                time_value_count++;
                //pr_info("Elapsed time: %lld ns\n", elapsed_time);
            }
            break;
        case IOCTL_SIGNAL_CLOSE:
            {
                pr_info("Среднее время: %lld ns\n", sum_time / time_value_count);
                pr_info("Максимальное время: %lld ns\n", max_time);
                
                
                // hist
                for(int i=0; i<bins; i++){
                    hist[i] = 0;
                }

                s64 step = (max_time - min_time) / bins;
                for(int i=0; i<time_value_count; i++){
                    for(int k=0; k<bins; k++){
                        if(history_time[i] >= min_time+(step*k) && history_time[i] < min_time+(step*(k+1)))
                            hist[k]++;
                    }
                    if(max_time == history_time[i])
                        hist[bins-1]++;
                }

                for(int k=0; k<bins; k++){
                    pr_info("bins: %lld-%lld ns:\t %lld\n", min_time+(step*k), min_time+(step*(k+1)), hist[k]);
                }
                

                min_time = 0;
                max_time = 0;
                sum_time = 0;
                time_value_count = 0;
            }
            break;
        default:
            return -EINVAL;
    }
    return 0;
}

static struct file_operations fops = {
    .open = device_open,
    .release = device_release,
    .unlocked_ioctl = device_ioctl,
};

static int __init my_driver_init(void) {
    struct device* dev;
    major = register_chrdev(0, DEVICE_NAME, &fops);

    if (major < 0) {
        pr_alert("register_chrdev() failed: %d\n", major);
        return -EINVAL;
    }

    pr_info("major = %d\n", major);

    cls = class_create(DEVICE_NAME);
    if (IS_ERR(cls)) {
        pr_alert("class_create() failed: %ld\n", PTR_ERR(cls));
        return -EINVAL;
    }

    dev = device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(dev)) {
        pr_alert("device_create() failed: %ld\n", PTR_ERR(dev));
        return -EINVAL;
    }

    pr_info("/dev/%s created\n", DEVICE_NAME);

    timer_setup(&timer_, timer_callback, 0);
    mod_timer(&timer_, jiffies + msecs_to_jiffies(timer_duration));

    return 0;
}

static void __exit my_driver_exit(void) {
    device_destroy(cls, MKDEV(major, 0));
    class_destroy(cls);
    unregister_chrdev(major, DEVICE_NAME);
    del_timer(&timer_);
}

module_init(my_driver_init);
module_exit(my_driver_exit);
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Simple ioctl driver");
MODULE_AUTHOR("Expert");
