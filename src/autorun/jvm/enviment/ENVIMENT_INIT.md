# hotspot 调试环境搭建

## openjdk 编译步骤
### 以jdk1.8为例
1. 安装Ubuntu环境(我这里使用20.04版本，不过版本最好高于16+以上，x86_64环境)
2. 使用终端更新依赖
> `sudo apt upgrade & sudo apt update`
> 如果是低版本的系统，只要把命令更换为 `sudo apt-get upgrade & sudo apt-get update`
3. 更新完毕之后安装必要的依赖库
> `sudo apt install libx11-dev libxext-dev libxrender-dev libxtst-dev libxt-dev libcups2-dev libfreetype6-dev libasound2-dev ccache`

> `apt` 命令同理
4. 安装 `git` (推荐) 或者 `hg` 工具用于克隆代码(二选其一即可)
> sudo apt install git / sudo apt install
> 
安装 `mercurial` (也就是hg命令，同上，二选其一即可)
```shell
sudo add-apt-repository -y ppa:mercurial-ppa/releases
sudo apt update
sudo apt install -y python-pip python-dev
sudo pip install mercurial --upgrade
```
> git 仓库地址: `https://github.com/openjdk/jdk/tree/jdk8-b120`

> hg 仓库地址: `http://hg.openjdk.java.net/jdk8u/jdk8u/`
5. 克隆 `openjdk` 源码
如果是git用户, 使用 `git clone -b jdk8-b120 git@github.com:openjdk/jdk.git` 命令克隆(推荐)

如果是hg用户则使用 `hg clone http://hg.openjdk.java.net/jdk8u/jdk8u/` </br>
即可以下载到openjdk的源码.
下载完成之后cd 到对应目录中
6. 给 `configure` 脚本增加执行权限
> chmod u+x ./configure

7. 下载bootJDK.
> 使用Oracle的jdk7或者openjdk皆可, 我这里选择Oracle的jdk</br>
> jdk下载地址: [下载链接](http://jdk.java.net/java-se-ri/7)

8. 配置 `bootJDK` 环境变量(将 `{base_path}` 替换成你自己的路径即可)

`sudo vim /etc/profile` # 将配置追加到你的profile文件末尾。使用 `G` 命令在vim的命令模式下快速跳转到文件末尾。然后使用 `O` 插入
```shell
export JAVA_HOME={base_path}/jdk1.7.0_80
export CLASSPATH=.:$JAVA_HOME/lib/dt.jar:$JAVA_HOME/lib/tools.jar
export PATH=$JAVA_HOME/bin:$ANT_HOME/bin:$PATH
```
然后使用 `:wq` 保存, 使用 `source` 命令使配置生效 `source /etc/profile`

9. 准备编译openJDK环境</br>
> `./configure --with-target-bits=64 --with-boot-jdk={base_path}/jdk1.7.0_80 --with-debug-level=slowdebug --enable-debug-symbols ZIP_DEBUGINFO_FILES=0`</br>

附录中会贴出 `configure` 命令所支持的参数。现在可以先这么用</br>
如果出现如下提示则配置成功
![img](./img/config_succ.png)

10. 开始编译openJDK</br>
> `make all DISABLE_HOTSPOT_OS_VERSION_CHECK=OK ZIP_DEBUGINFO_FILES=0` </br>
如果出现如下提示则编译成功
![img](./img/build_succ.png)

11. 编译中可能出现的一些问题
    1. 报错信息1(这是我编译时候遇到最多的问题。后续有问题可以提issue. 我这边更新到文档中)
    ```c++ 
    make[6]: *** [/home/autorun/platform/source/jdk/hotspot/make/linux/makefiles/vm.make:297: precompiled.hpp.gch] Error 1
    make[5]: *** [/home/autorun/platform/source/jdk/hotspot/make/linux/makefiles/top.make:119: the_vm] Error 2
    make[4]: *** [/home/autorun/platform/source/jdk/hotspot/make/linux/Makefile:289: product] Error 2
    make[3]: *** [Makefile:217: generic_build2] Error 2
    make[2]: *** [Makefile:167: product] Error 2
    make[1]: *** [HotspotWrapper.gmk:45: /home/autorun/platform/source/jdk/build/linux-x86_64-normal-server-release/hotspot/_hotspot.timestamp] Error 2
    make: *** [/home/autorun/platform/source/jdk//make/Main.gmk:109: hotspot-only] Error 2
    ``` 
        解决方案: 降低g++和gcc版本即可。
    ```shell
    sudo apt install -y gcc-4.8
    sudo apt install -y g++-4.8
    # 重新建立软连接
    cd /usr/bin             #进入/usr/bin文件夹下
    sudo rm -r gcc          #移除之前的软连接
    sudo ln -sf gcc-4.8 gcc #建立gcc4.8的软连接
    sudo rm -r g++
    sudo ln -sf g++-4.8 g++
    ```

12. 关于 `configure` 脚本和 `make` 


1


1


1


1