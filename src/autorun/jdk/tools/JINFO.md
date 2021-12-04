# JInfo 使用技巧

## 命令详解
### 1. 命令描述
#### 1.1 命令格式

> `jinfo [ option ] pid `</br>
> `jinfo [ option ] executable core` </br>
> `jinfo [ option ] [server-id@]remote-hostname-or-IP`

#### 1.2 简介
> jinfo prints Java configuration information for a given Java process or core file or a remote debug server. Configuration information includes Java System properties and Java virtual machine command line flags. If the given process is running on a 64-bit VM, you may need to specify the -J-d64 option, 

>e.g.: jinfo -J-d64 -sysprops pid
>
> 参考链接 [jdk8_jinfo](https://docs.oracle.com/javase/8/docs/technotes/tools/unix/jinfo.html#BCGEBFDD)

大意就是 `jinfo` 是java虚拟机自带的Java配置信息工具，可以为一个给定的Java进程或核心文件或一个远程调试服务器打印Java配置信息。
配置信息包括Java系统属性和Java虚拟机命令行标志。</br>
如果给定的进程在64位虚拟机上运行，你可能需要指定 -J -d64选项，例如: jinfo -J -d64 -sysprops pid






| type |            property name        |  default |     level    |
| ---  |              ---                |   ---    |      ---     |
| intx | CMSAbortablePrecleanWaitMillis  |  = 100   | {manageable} |
| intx | CMSTriggerInterval              |  = -1    | {manageable} |
| intx | CMSWaitDuration                 |  = 2000  | {manageable} |
| bool | HeapDumpAfterFullGC             |  = false | {manageable} |
| bool | HeapDumpBeforeFullGC            |  = false | {manageable} |
| bool | HeapDumpOnOutOfMemoryError      |  = false | {manageable} |
|ccstr | HeapDumpPath                    |  =       | {manageable} |
|uintx | MaxHeapFreeRatio                |  = 70    | {manageable} |
|uintx | MinHeapFreeRatio                |  = 40    | {manageable} |
| bool | PrintClassHistogram             |  = false | {manageable} |
| bool | PrintClassHistogramAfterFullGC  |  = false | {manageable} |
| bool | PrintClassHistogramBeforeFullGC |  = false | {manageable} |
| bool | PrintConcurrentLocks            |  = false | {manageable} |
| bool | PrintGC                         |  = false | {manageable} |
| bool | PrintGCDateStamps               |  = false | {manageable} |
| bool | PrintGCDetails                  |  = false | {manageable} |
| bool | PrintGCID                       |  = false | {manageable} |
| bool | PrintGCTimeStamps               |  = false | {manageable} |