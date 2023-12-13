# CMU15-445/2022Fall 记录

---

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

---

## Project 0 C++ Primer

### 踩过的各种坑

- 首先是做整个项目得先仔细把 `Porject0` 的 `instructions` 部分看了

- `Testing` 部分是实现本地测试的，按命令走就可以了，但要使用测试样例时，需要把对应test文件的测试样例前面的 `DISABLED_` ，否则无法测试

- `Formatting` 部分的指令直接执行会报错，实际上是文件权限不够，可以用下列命令赋予文件权限，之后就可以编译了；另外一定要搞好整个程序的语法和格式部分，本地测试通过了但线上测试是零分，原因大概率是因为语法和格式不符合要求

  ```
  sudo chmod -R 777 myResources
  ```

- `Development Hints` 里提到的输出函数可以帮忙调试，但也可以用 `gdb`，本菜狗+懒人没学，就直接用给的输出函数了，至少目前用用没啥问题

- 提交方式，在最后的 `SUBMISSION` 提到了，作业只能通过 `zip` 文件的方式提交，不能用 `github` 提交，且 `zip` 最好使用提供的命令，因为提交的文件需要包括路径

## Project 0 本身

项目要求实现的是一个 `Trie` 树，不熟悉的可以先看看 [208. 实现 Trie (前缀树)](https://leetcode.cn/problems/implement-trie-prefix-tree/)

实现的逻辑不难，但不熟悉C++语言的我在处理各种语法问题上磨了很久，具体而言，完成这个 `Project` 需要了解以下语法知识：

- 左值引用和右值引用（右值引用特别多）
- `std::unique_ptr<T>>` 唯一指针的使用
- `std::move` 强制将左值转为右值的引用
- `std::make_unique<T>()` 的使用，注意T是对象的时候，括号里的参数填的是T对象构造函数的参数（在这里卡了很久……）
- 移动构造函数，拷贝构造函数等
- `dynamic_cast`
- `ReaderWriterLatch` 锁（这个简单了解即可，会调用项目已经写好的函数就行）

项目的其他问题：

- 实现前一定要仔细阅读注释的要求（但这里的坑也不多，我就踩过一个，`Trie` 类的 `GetValue` 函数，使用`dynamic_cast` 后得到的指针只要不为空就可以认为最终的 `value` 和函数的 `T` 是相同的，不需要继续判断）
- 变量命名必须使用驼峰命名法
- 线上测试出现 `Test Failed: False is not true : Test was not run. Please check the autograder!`，有两种可能的原因，一是语法或格式不符合 `clang-tidy` 的要求，二是发生了内存泄露（函数执行过程中出现了死循环或者访问空指针）

别的想起来继续补充，这里放一张线上测试满分的图

<img src="mySources\scores\Project 0 Score.jpg" style="zoom: 67%;" />
