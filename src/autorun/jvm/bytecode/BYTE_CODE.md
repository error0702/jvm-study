# Java 字节码规范

## 1. 字节码常见指令
| 操作码 | 助记符 | 释义 |
| ---| --- | --- |
| 00 | nop | 什么都不做|
| 01 | aconst_null | 将 `null` 推送到栈顶 |
| 02 | iconst_m1 | 将整数 `-1` 推送至栈顶 |
| 03 | iconst_0 | 将int类型 `0` 推送至栈顶 |
| 04 | iconst_1 | 将int类型 `1` 推送至栈顶 |
| 05 | iconst_2 | 将int类型 `2` 推送至栈顶 |
| 06 | iconst_3 | 将int类型 `3` 推送至栈顶 |
| 07 | iconst_4 | 将int类型 `4` 推送至栈顶 |
| 08 | iconst_5 | 将int类型 `5` 推送至栈顶 |
| 09 | lconst_0 | 将long类型 `0` 推送至栈顶 |
| 10 | lconst_1 | 将long类型 `1` 推送至栈顶 |
| 11 | fconst_0 | 将float类型 `0` 推送至栈顶 |
| 12 | fconst_1 | 将float类型 `1` 推送至栈顶 |
| 13 | fconst_2 | 将float类型 `2` 推送至栈顶 |
| 14 | dconst_0 | 将double类型 `0` 推送至栈顶 |
| 15 | dconst_1 | 将double类型 `1` 推送至栈顶 |
| 16 | bipush | 将单字节常量值(-128~127) 推送至栈顶 |
