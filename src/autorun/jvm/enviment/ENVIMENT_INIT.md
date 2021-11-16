# hotspot 调试环境搭建

## openjdk 编译步骤
1. 安装Ubuntu环境(我这里使用20.04版本，不过版本最好高于16+以上，x86_64环境)
2. 使用终端更新依赖
> `sudo apt upgrade & sudo apt update`
> 如果是低版本的系统，只要把命令更换为 `sudo apt-get upgrade & sudo apt-get update`
3. 更新完毕之后安装必要的依赖库
> `sudo apt install libx11-dev libxext-dev libxrender-dev libxtst-dev libxt-dev libcups2-dev libfreetype6-dev libasound2-dev ccache`

> `apt` 命令同理
4. 