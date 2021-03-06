#include <stdio.h>
#include <stdlib.h>
#include "mpc.h"
#ifdef _WIN32 // 预处理做环境判断, windows环境下使用自己实现的函数
#include <string.h>

static char buffer[2048]; // 定义一个固定大小的数组缓冲区

char *readline(char *prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);

    char *cpy = malloc(strlen(buffer)+1); // 预先开辟堆区内存(size：buffer char个数加1bit)
    strcpy(cpy, buffer); // 将buffer指针变量所指向的字符串复制到cpy指针变量所指向的空间去，为防止堆区内存缓冲溢出的错误，开辟内存加1
    cpy[strlen(cpy) - 1] = '\0'; //将cpy指针变量最后一位地址作为字符串结束符（其实strcpy会把字符串结束符'0\'复制过去）
    return cpy; // 将cpy字符串作为返回值
}

void add_history(char *unused) {} // 暂时不实现

#else // linux环境下就调用readline库
#include <readline/readline.h>
#include <readline/history.h>
#endif

typedef struct lval { // 定义一个错误处理的结构体类型
    int type; // 表示结果的类型
    long num;
    int err;
} lval;
enum { LVAL_NUM, LVAL_ERR }; // 枚举用于type：0 or 1
enum { LERR_DIV_ZERO, LERR_BAD_OP, LERR_BAD_NUM }; // 用于描述err

lval lval_num(long x) { // 定义lval结构体变量函数
  lval v;
  v.type = LVAL_NUM;
  v.num = x;
  return v;
}

lval lval_err(int x) { // 定义lval结构体变量函数
  lval v;
  v.type = LVAL_ERR;
  v.err = x;
  return v;
}

void lval_print(lval v) {
  switch (v.type) {
    /* In the case the type is a number print it */
    /* Then 'break' out of the switch. */
    case LVAL_NUM: printf("%li", v.num); break;

    /* In the case the type is an error */
    case LVAL_ERR:
      /* Check what type of error it is and print it */
      if (v.err == LERR_DIV_ZERO) {
        printf("Error: Division By Zero!");
      }
      if (v.err == LERR_BAD_OP)   {
        printf("Error: Invalid Operator!");
      }
      if (v.err == LERR_BAD_NUM)  {
        printf("Error: Invalid Number!");
      }
    break;
  }
}

/* Print an "lval" followed by a newline */
void lval_println(lval v) { lval_print(v); putchar('\n'); }

int number_of_nodes(mpc_ast_t *t){ // 递归解析mpc_ast_t数据结构体
  if(t->children_num == 0){ return 1;}
  if(t->children_num >= 1){
      int total = 1;
      for (int i=0;i<t->children_num;i++)
      {
          total = total + number_of_nodes(t->children[i]);
    }
    return total;
}
    return 0;
    /*实际上这句没有必要*/
}

/*
long eval_op(long x, char *op, long y) {
    if(strcmp(op, "+") == 0){return x + y;}
    if(strcmp(op, "-") == 0){return x - y;}
    if(strcmp(op, "*") == 0){return x * y;}
    if(strcmp(op, "/") == 0){return x / y;}
    return 0;
}
*/

lval eval_op(lval x, char *op, lval y) {
    /* If either value is an error return it */
    if (x.type == LVAL_ERR) { return x; }
    if (y.type == LVAL_ERR) { return y; }
    /* Otherwise do maths on the number values */
    if (strcmp(op, "+") == 0) { return lval_num(x.num + y.num); }
    if (strcmp(op, "-") == 0) { return lval_num(x.num - y.num); }
    if (strcmp(op, "*") == 0) { return lval_num(x.num * y.num); }
    if (strcmp(op, "/") == 0) {
    /* If second operand is zero return error */
        return y.num == 0 ? lval_err(LERR_DIV_ZERO) : lval_num(x.num / y.num);
    } 

    return lval_err(LERR_BAD_OP);
}

lval eval(mpc_ast_t *t) {
    // 利用递归来分析ouput char* 长字符串tag
    // + 1 (+ 2 4) output的到的char *
    /*
    >
    regex
    operator|char:1:1 '+'
    expr|number|regex:1:3 '1'
    expr|>
        char:1:5 '('
        operator|char:1:6 '+'
        expr|number|regex:1:8 '2'
        expr|number|regex:1:10 '4'
        char:1:11 ')'
    regex 
    */
    if (strstr(t->tag, "number")){
        // return atoi(t->contents); // char *转为long长整型
        errno = 0; // errno.h
        long x = strtol(t->contents, NULL, 10); //strtol 函数进行字符串到数字的转换，因为可以通过检测 errno 变量确定是否转换成功。10进制
        return errno != ERANGE ? lval_num(x) : lval_err(LERR_BAD_NUM);
    }
    char *op = t->children[1]->contents;
    lval x = eval(t->children[2]);
    int i = 3;
    while (strstr(t->children[i]->tag, "expr")){
        x = eval_op(x, op, eval(t->children[i]));
        i++;
    }
    return x;
}

int main(int argc, char *argv[]) {

    mpc_parser_t *Number    = mpc_new("number"); // 由0-9数字组成，也可以用'-'来表示负数
    mpc_parser_t *Operator  = mpc_new("operator"); // '+', '-', '*', 或者 '/'
    mpc_parser_t *Expr      = mpc_new("expr"); // 由一个数字或者 '(' 操作符+其它的表达式 ')', 一个表达式
    mpc_parser_t *Lispy     = mpc_new("lispy"); // 输入由操作符，一个或者多个表达式组成

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                   \
        number   : /-?[0-9]+/ ;                             \
        operator : '+' | '-' | '*' | '/' ;                  \
        expr     : <number> | '(' <operator> <expr>+ ')' ;  \
        lispy    : /^/ <operator> <expr>+ /$/ ;             \
        ",
        Number, Operator, Expr, Lispy);
    /* 
    第一个参数是选择标志，我们使用默认值即可，
    第二个是一个长点的字符串，它是指语法说明，包含了很多的重写规则。每一条规则都有左边的 : 和右边的 ; 组成。特殊的符号定义的规则如下所示： 
    "ab"    表示字符串 ab
    'a'        表示字符 a
    'a' 'b'    表示先有字符 a ,再有字符 b
    'a' | 'b' 表示在字符 a 和 b 之间选择一个    
    'a'*    表示有0个或多个a
    'a'+    表示至少有一个a
    <abba>    表示规则 abba
    */
    puts("Lispy Version 0.3");
    puts("Press Ctrl+c to Exit\n");

    while (1)
    {
        char *input = readline("Lispy> ");
        add_history(input);

        /* Attempt to parse the user input */
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            /* On success print and delete the AST */
            // mpc_ast_t* a = r.output;
            // printf("Tag: %s\n", a->tag);
            // printf("Contents: %s\n", a->contents);
            // printf("Number of children: %i\n", a->children_num);

            // mpc_ast_t* c0 = a->children[0];
            // printf("First Child Tag: %s\n", c0->tag);
            // printf("First Child Contents: %s\n", c0->contents);
            // printf("First Child Number of children: %i\n" , c0->children_num);
            // mpc_ast_print(r.output);
            // mpc_ast_delete(r.output);
            // number_of_nodes(r.output);
            // long result = eval(r.output);
            // printf("%li\n", result);
            lval result = eval(r.output);
            lval_println(result);
            mpc_ast_delete(r.output);
        } else {
            /* Otherwise print and delete the Error */
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }

        free(input); // 释放input内存
    }

    /* Do some parsing here... */
    mpc_cleanup(4, Number, Operator, Expr, Lispy);

    return 0;
}
