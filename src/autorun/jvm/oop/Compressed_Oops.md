# CompressedOops 指针压缩技术(X64系统)

> 指针压缩技术是针对64位系统的。在 `oop` 模型下，定义的 `java` 对象(`oop`对象) 和 class对象 (`instanceMinorKlass` 对象) 的关系是
> 
> `oop` 对象中 除去对象头、填充值和字段数据(成员变量)之外还要存储 `klass` 对象对应的指向。方便获取对应类型。同时，`klass` 对象中还存放着与 `oop` 对象关联度很高的很多属性和操作方法(多态表，`vtable`、`itable`)。
## JAVA对象布局
![](img/compressed_oops_1.png)