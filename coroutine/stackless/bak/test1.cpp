#include <stdio.h>

int function(void) {
    static int i, state = 0;
    switch (state) {
        case 0: goto LABEL0;
        case 1: goto LABEL1;
    }
    LABEL0: // 函数开始执行
    for (i = 0; i < 10; i++) {
        state = 1; // 我们会跳转到 LABEL1
        return i;
        LABEL1:; // 从上一次返回之后的地方开始执行
    }
}

int function_d(void) {
    static int i, state = 0;
    switch (state) {
        case 0: // 函数开始执行
        for (i = 0; i < 10; i++) {
            state = 1; // 我们会回到 "case 1" 的地方
            return i;
            case 1:; // 从上一次返回之后的地方开始执行
        }
    }
}

#define crBegin static int state=0; switch(state) { case 0:
#define crReturnBase(i,x) do { state=i; return x; case i:; } while (0)

#define crReturn(x) do { state=__LINE__; return x; \
                         case __LINE__:; } while (0)

#define crFinish }

int function(void) {
    static int i;
    crBegin;
    for (i = 0; i < 10; i++) {
        crReturnBase(1, i);
    }
    crFinish;
}

int decompressor(void) {
    static int c, len;
    crBegin;
    while (1) {
        c = getchar();
        if (c == EOF) break;
        if (c == 0xFF) {
            len = getchar();
            c = getchar();
            while (len--) {
                crReturn(c);
            }
        } else {
            crReturn(c);
        }
    }
    crReturn(EOF);
    crFinish;
}

void parser(int c) {
    crBegin;
    while (1) {
        // 第一个字符已经存储在 c 中了
        if (c == EOF) break;
        if (isalpha(c)) {
            do {
                add_to_token(c);
                crReturn( );
            } while (isalpha(c));
            got_token(WORD);
        }
        add_to_token(c);
        got_token(PUNCT);
        crReturn( );
    }
    crFinish;
}