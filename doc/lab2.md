## Tiger-Compiler Lab2

Author: 519021910390 杨镇宇

### Interesting Feature

关于不同 StartCondition 之间的状态转换，观察到这一状态满足先进后出的规则，于是维护了一个状态栈，用以实现统一的状态管理，而不用记录 comment_level_ 等冗余信息：

```c++
std::stack<StartCondition__> d_scStack;

/* push and pop StartCondition */
void pushCond(StartCondition__ next);
void popCond();
```

### Comment Handling

在 **INITIAL** 的 StartCondition下匹配到 "/\*" 即向栈上 push 一个 **COMMENT** sc，此后每次匹配到 "/\*" 会继续 `pushCond(StartCondition__::COMMENT)` ； 匹配到 "\*/" 则会 `popCond()` ，其余字符模式则被忽略。

在这一实现下，匹配到 "\*/" 时不用特殊判断是否要返回 **INITIAL** ，状态栈会自动处理。

### String Handling

**STR** 这一 StartCondition 的进入和退出与 **COMMENT** 类似，都由状态栈维护，不再赘述。

相比于 **COMMENT** ， **STR** 维护了一个缓存字符串 (string_buf_ ) 来保存一个 String 中的所有信息；处理的难点在于其中的各种转义字符和需要 **IGNORE** 的字段。

+ 当匹配到转义字符时，向 string_buf_ 中加入转义后的字符 （包括 `\\[[:digit:]]{3}` 模式)

+ 匹配到 **\\** 时进入 **IGNORE** StartCondition，其中的字符都不会进入 string_buf_ （被忽略了）

### Error Handling

Lab2 中大致出现了两类错误：非法输入和 Unclosed StartCondition。

+ 所有未匹配 lex 文件中定义的 pattern 即为非法输入，在 lex 文件末尾用 **.** 通配符接收；
+ 在非 **INITIAL** StartCondition 下读到 <<**EOF**>> 均为 Unclosed StartCondition；

检测到错误后，用 `errormsg_->Error(errormsg_->tok_pos_, "error message")` 报错。报错后可通过 `adjust()` 函数跳过非法输入继续进行 parse 操作。

### End-Of-File Handling

如上所说，在非 **INITIAL** StartCondition 下读到 <<**EOF**>> 均为 Unclosed StartCondition。这在 lex 文件中可以这样匹配：

```less
<STR, COMMENT, IGNORE><<EOF>> {adjust(); /* report error code */}
```
