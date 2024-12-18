#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <stdlib.h>

#define DEVICE_NAME "/dev/mydevice"
#define IOCTL_SET_PID _IOW('a', 'b', pid_t)
#define IOCTL_TRIGGER_APP _IO('a', 'c')
#define IOCTL_SIGNAL_RECEIVED _IO('a', 'd')
#define IOCTL_SIGNAL_CLOSE _IO('a', 'e')


int fd;

void signal_handler(int signum) {
    printf("Received signal from driver!\n");
    // Отправляем сигнал обратно в драйвер
    if (fd < 0) {
        perror("Failed to open device");
        return;
    }
    ioctl(fd, IOCTL_SIGNAL_RECEIVED); // Уведомляем драйвер о получении сигнала
}

void exit_handler(int signum) {
    printf("Exit!\n");

    if (fd < 0) {
        perror("Failed to open device");
        return;
    }
    ioctl(fd, IOCTL_SIGNAL_CLOSE); // Уведомляем драйвер о получении сигнала

    close(fd);
    exit(0);
}



int main() {
    fd = open(DEVICE_NAME, O_RDWR);
    if (fd < 0) {
        perror("Failed to open device");
        return -1;
    }

    signal(SIGUSR1, signal_handler);
    signal(SIGINT, exit_handler); 
    
    pid_t my_pid = getpid();
    ioctl(fd, IOCTL_SET_PID, &my_pid); // Устанавливаем свой PID

    // Ожидание сигнала от драйвера
    while (1) {
        pause(); // Ожидаем получения сигнала
    }

    close(fd);
    return 0;
}
