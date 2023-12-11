# CMU15-445/2022Fall 记录

## 配环境踩过的坑

注意：使用的Ubuntu18.04版本，且已安装好；坑的部分不是很全，只记录了些我觉得比较重要，而且自己还有印象的部分（可能通关后会试着重装一遍，到时再补充）

### `git` 部分

做2022fall版本，需要找到CMU原仓库对应的 `tag` 后才能 `git clone`，否则下载的是master分支，也就是今年CMU15-445正在用的分支，而不是2022fall版本

2022fall的分支tag是 `v20221128-2022fall`，先将其克隆到本地：

```
git clone -branch v20221128-2022fall git@github.com:cmu-db/bustub.git
```

之后按官方的教程走，是不能直接推送仓库到自己的远程仓库的，需要在当前仓库执行以下命令

```
git config --global --add safe.directory "*"
```

然后在自己的远程仓库新建一个空仓库（不要创建README.md文件），将仓库通过以下命令推送到自己的远程仓库（官方的命令直接使用会有错误）

```
git push git@github.com:NidoSen/CMU15-445.git HEAD:refs/heads/master
```

后续就是按教程走了，先把本地仓库删除，然后将远程仓库 `git clone` 到本地，就可以开始后续配环境的操作了

### 安装部分

2022fall的CMU15-445用的是 `clang-12`，网上找的安装教程有些不一定适合，我用的是[清华的镜像](https://mirrors4.tuna.tsinghua.edu.cn/help/llvm-apt/)，其实中间还有些换源的操作，这些操作网上教程容易找到，就不赘述了

```
wget https://mirrors.tuna.tsinghua.edu.cn/llvm-apt/llvm.sh
chmod +x llvm.sh
./llvm.sh all -m https://mirrors.tuna.tsinghua.edu.cn/llvm-apt
```

然后记得升级下 `gcc/g++`，版本过低在 `make` 阶段会有各种恶心的错误，我升级到了11版本

之后按教程执行以下语句

```
sudo bash build_support/packages.sh
```

会报错：

```
'\r': command not found [duplicate]
```

原因和Windows和Linux的文件格式有关，可以按命令重新生成一个去掉 `\r` 的 `packages_new.sh` 文件，并重新执行

```
cd build
tr -d "\r" < packages.sh > packages_new.sh
cd ..
sudo bash build_support/packages_new.sh
```

之后走完教程就能编译通过了

