#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(int argc, char *argv[]) {
    pid_t pid;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <command> <arg>\n", argv[0]);
        exit(1);
    }

    pid = fork();

    // forkが失敗した場合、子プロセスは生成されずエラーメッセージだけ出力して終了する
    if (pid < 0) {
        fprintf(stderr, "fork(2) failed\n");
        exit(1);
    }

    if (pid == 0) {
        // 子プロセスの処理
        execl(argv[1], argv[1], argv[2], NULL);
        // execlはエラー時以外は返ってこないのでココに到達した時点でエラー終了させる
        perror(argv[1]);
        exit(1);
    } else {
        // 親プロセスの処理
        int status;
        printf("child (PID=%d) started\n", pid);

        // 子プロセスの終了を待つ
        waitpid(pid, &status, 0);
        printf("child (PID=%d) finished\n", pid);

        // 子プロセスの終了理由を出力
        if (WIFEXITED(status)) {
            printf("exit, status=%d (PID=%d)\n", WEXITSTATUS(status), pid);
        } else if (WIFSIGNALED(status)) {
            printf("signal, sig=%d (PID=%d)\n", WTERMSIG(status), pid);
        } else {
            printf("abnormal exit (PID=%d)\n", pid);
        }
        exit(0);
    }
}
