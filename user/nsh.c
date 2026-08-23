#include <stdint.h>
#include <user/user.h>
#include <user/fcntl.h>

#define MAXARGS 10


enum cmd_type {
    CMD_EXEC = 1,
    CMD_REDIR,
    CMD_PIPE,
    CMD_LIST,
    CMD_BACK,
};

struct cmd {
    enum cmd_type type;
};

struct exec_cmd {
    enum cmd_type type;
    char *argv[MAXARGS];
    char *eargv[MAXARGS];
};

struct redir_cmd {
    enum cmd_type type;
    struct cmd *cmd;
    char *fname;
    char *fname_end;
    int mode;
    int fd; // fd to close
};

struct pipe_cmd {
    enum cmd_type type;
    struct cmd *left;
    struct cmd *right;
};

struct list_cmd {
    enum cmd_type type;
    struct cmd *left;
    struct cmd *right;
};

struct back_cmd {
    enum cmd_type type;
    struct cmd *cmd;
};

int64_t fork1(void);
void panic(char*);
struct cmd* parse_cmd(char*);

void run_cmd(struct cmd *cmd) {
    // nvr return
    int p[2];

    if(cmd == 0) exit();

    switch(cmd->type) {
        case CMD_EXEC:
            struct exec_cmd *ecmd = (void*) cmd;
            if(ecmd->argv[0] == 0) exit(); 
            exec(ecmd->argv[0], ecmd->argv);
            printf(2, "exec %s failed\n", ecmd->argv[0]);
            break;
        case CMD_REDIR:
            struct redir_cmd *rcmd = (void*) cmd;
            close(rcmd->fd); // fd to close
            if(open(rcmd->fname, rcmd->mode) < 0) { // get the lowest fd, which was held by stdout
                printf(2, "open %s failed\n", rcmd->fname);
                exit();
            }
            run_cmd(rcmd->cmd);
            break;
        case CMD_LIST:
            struct list_cmd *lcmd = (void*) cmd;
            if(fork1() == 0) {
                run_cmd(lcmd->left);
            }
            wait();
            run_cmd(lcmd->right);
            break;
        case CMD_PIPE:
            struct pipe_cmd *pcmd = (void*) cmd;
            if(pipe(p) < 0) {
                panic("pipe failed");
            }

            if(fork1() == 0) {
                close(1); // close stdout
                dup(p[1]); // 1 becomes the pipe write file
                close(p[0]); // close pipe read fd
                close(p[1]); // close pipe write fd
                run_cmd(pcmd->left);
            }

            if(fork1() == 0) {
                close(0); // close stdin
                dup(p[0]); // 0 becomes the pipe read file
                close(p[0]);
                close(p[1]);
                run_cmd(pcmd->right);
            }

            close(p[0]);
            close(p[1]);
            wait();
            wait();
            break;
        case CMD_BACK:
            struct back_cmd *bcmd = (void*) cmd;
            if(fork1() == 0) {
                run_cmd(bcmd->cmd);
            }
            break;
        default:
            panic("run_cmd");
    }
    exit();
}

// fill buf with cmd
int get_cmd(char *buf, int nbuf) {
    printf(2, "$ "); // stdout may vary to any other file, so use stderr
    memset(buf, 0, nbuf);
    gets(buf, nbuf);
    if(buf[0] == 0) return -1;
    return 0;
} 

int main(void) {
    static char buf[128];

    // ensure three fd
    int fd;
    while((fd = open("/console", O_RDWR)) >= 0) {
        if(fd >= 3) {
            close(fd);
            break;
        }
    }

    while(get_cmd(buf, sizeof(buf)) >= 0) {
        if(buf[0] == 'c' && buf[1] == 'd' && buf[2] == ' ') {
            buf[strlen(buf)-1] = 0;
            if(chdir(buf+3) < 0) {
                printf(2, "cannot cd %s", buf+3);
            }
            continue;
        }

        if(fork1() == 0) {
            run_cmd(parse_cmd(buf));
        }

        wait();
    }
    exit();
}

void panic(char *s) {
    printf(2, "%s\n", s);
    exit();
}

int64_t fork1(void) {
    int64_t pid = fork();
    if(pid == -1) panic("fork");
    return pid;
}

// Constructors
struct cmd* exec_cmd(void) {
    struct exec_cmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_EXEC;
    return cmd;
}

struct cmd* redir_cmd(struct cmd *sub_cmd, char *fname, char *fname_end, int mode, int fd) {
    struct redir_cmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_REDIR;
    cmd->cmd = sub_cmd;
    cmd->fname = fname;
    cmd->fname_end = fname_end;
    cmd->mode = mode;
    cmd->fd = fd;
    return cmd;
}

struct cmd* pipe_cmd(struct cmd *left, struct cmd *right) {
    struct pipe_cmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_PIPE;
    cmd->left = left;
    cmd->right = right;
    return cmd;
}

struct cmd* list_cmd(struct cmd *left, struct cmd *right) {
    struct list_cmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_LIST;
    cmd->left = left;
    cmd->right = right;
    return cmd;
}

struct cmd* back_cmd(struct cmd *sub_cmd) {
    struct back_cmd *cmd = malloc(sizeof(*cmd));
    memset(cmd, 0, sizeof(*cmd));
    cmd->type = CMD_BACK;
    cmd->cmd = sub_cmd;
    return cmd;
}

// recursive descent parser

char white_space[] = " \t\r\n\v";
char symbols[] = "<|>&;()";

int get_token(char **str_p, char *str_end, char **token_p, char **token_end_p) {
    char *str = *str_p;
    while(str < str_end && strchr(white_space, *str)) str++;
    if(token_p) *token_p = str;

    int ret = *str;

    switch(*str) {
        case 0 : break;
        case '|':
        case '(':
        case ')':
        case ';':
        case '&':
        case '<':
            str++;
            break;
        
        case '>':
            str++;
            if(*str == '>') {
                ret = '+';
                str++;
            }
            break;
        
        default:
            ret = 'a';
            while(str < str_end && !strchr(white_space, *str) && !strchr(symbols, *str)) {
                str++;
            }
            break;
    }
    if(token_end_p) *token_end_p = str;

    while(str < str_end && strchr(white_space, *str)) str++;
    *str_p = str;

    return ret;
}

int peek(char **str_p, char *str_end, char *toks) {
    char *str = *str_p;

    while(str < str_end && strchr(white_space, *str))
        str++;

    *str_p = str;
    return *str && strchr(toks, *str);
}

struct cmd* parse_line(char**, char*);
struct cmd* parse_pipe(char**, char*);
struct cmd* parse_exec(char**, char*);
struct cmd* null_terminate(struct cmd*);
struct cmd* parse_block(char **str_p, char *str_end);
struct cmd* parse_redirs(struct cmd *cmd, char **str_p, char *str_end);


struct cmd* parse_cmd(char *str) {
    char *str_end = str + strlen(str);
    struct cmd *cmd = parse_line(&str, str_end);
    peek(&str, str_end, "");
    if(str != str_end) {
        printf(2, "leftover : %s\n", str);
        panic("syntax");
    }
    null_terminate(cmd);
    return cmd;
}

struct cmd* parse_line(char **str_p, char *str_end) {
    struct cmd *cmd = parse_pipe(str_p, str_end);
    while(peek(str_p, str_end, "&")) {
        get_token(str_p, str_end, 0, 0);
        cmd = back_cmd(cmd);
    }

    if(peek(str_p, str_end, ";")) {
        get_token(str_p, str_end, 0, 0);
        cmd = list_cmd(cmd, parse_line(str_p, str_end));
    }

    return cmd;
}

struct cmd* parse_pipe(char **str_p, char *str_end) {
    struct cmd *cmd = parse_exec(str_p, str_end);
    if(peek(str_p, str_end, "|")) {
        get_token(str_p, str_end, 0, 0);
        cmd = pipe_cmd(cmd, parse_pipe(str_p, str_end));
    }

    return cmd;
}

struct cmd* parse_exec(char **str_p, char *str_end) {
    if(peek(str_p, str_end, "("))
        return parse_block(str_p, str_end);

    struct cmd *ret = exec_cmd();
    struct exec_cmd *cmd = ret;

    int argc = 0;
    ret = parse_redirs(ret, str_p, str_end);

    char *q, *eq;
    while(!peek(str_p, str_end, "|)&;")) {
        int tok = get_token(str_p, str_end, &q, &eq);
        if(tok == 0) break;

        if(tok != 'a') panic("syntax");

        cmd->argv[argc] = q;
        cmd->eargv[argc] = eq;
        argc++;
        if(argc >= MAXARGS) panic("too many args");
        ret = parse_redirs(ret, str_p, str_end);
    }

    cmd->argv[argc] = 0;
    cmd->eargv[argc] = 0;
    return ret;

}

struct cmd* parse_redirs(struct cmd *cmd, char **str_p, char *str_end) {
    char *q, *eq;
    while(peek(str_p, str_end, "<>")) {
        int tok = get_token(str_p, str_end, 0, 0); // get < or > 
        if(get_token(str_p, str_end, &q, &eq) != 'a') 
            panic("missing file for redir");
        
        switch(tok) {
            case '<':
                cmd = redir_cmd(cmd, q, eq, O_RDONLY, 0);
                break;
            case '>':
                cmd = redir_cmd(cmd, q, eq, O_WRONLY | O_CREATE, 1);
                break;
            case '+':
                cmd = redir_cmd(cmd, q, eq, O_WRONLY|O_CREATE, 1);
                break;
        }
    }
    return cmd;
}

struct cmd* parse_block(char **str_p, char *str_end) {
    if(!peek(str_p, str_end, "(")) {
        panic("parse_block");
    }

    get_token(str_p, str_end, 0, 0);
    struct cmd *cmd = parse_line(str_p, str_end);
    if(!peek(str_p, str_end, ")")) {
        panic("syntax ) missing");
    }
    get_token(str_p, str_end, 0, 0);
    cmd = parse_redirs(cmd, str_p, str_end);
    return cmd;
}

struct cmd* null_terminate(struct cmd *cmd) {
    if(cmd == 0) return 0;

    switch(cmd->type) {
        case CMD_EXEC:
            struct exec_cmd *ecmd = cmd;
            for(int i=0; ecmd->argv[i]; i++) *ecmd->eargv[i] = 0;
            break;
        case CMD_REDIR:
            struct redir_cmd *rcmd = cmd;
            null_terminate(rcmd->cmd);
            *rcmd->fname_end = 0;
            break;
        case CMD_PIPE:
            struct pipe_cmd *pcmd = cmd;
            null_terminate(pcmd->left);
            null_terminate(pcmd->right);
            break;
        case CMD_LIST:
            struct list_cmd *lcmd = cmd;
            null_terminate(lcmd->left);
            null_terminate(lcmd->right);
            break;
        case CMD_BACK:
            struct back_cmd *bcmd = cmd;
            null_terminate(bcmd->cmd);
            break;
    }
    return cmd;
}


