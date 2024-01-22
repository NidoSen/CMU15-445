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

### Project 0 本身

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

---

## Project 1 - Buffer Pool

进入正题后，虽然Project1是整个项目的开始和最简单的部分，但也能明显感觉到，这个难度和Project0就不是一个级别的，断断续续做了几天，几乎每个Task都能让我头脑风暴（后面还有更难的3个project，希望脑细胞还够用）

### Task 1 - Extendible Hash Table

Task1的任务是实现一个可扩展散列表，个人认为这是整个Project1最难的部分，原因在于可扩展散列表的设计思路和我们平常接触的散列表是差别很大的，如果没有看过 **`Lecture #07: Hash Tables`**，是很难自己想对的（比较奇怪的**这节课是 `Project 1` 布置后才上的**，我以为 `Project 1` 布置的时候，需要的前置知识都讲完了，就没看后面的课直接做，那叫一个酸爽啊），这里就记录下曾经卡住我很长时间的几个点：

- `depth_` 和 `golbal_depth_` 的含义一定要理解正确，自己一开始以为 `depth_` 是单个容器 `dir_[i]` 里存储的键值对个数，而  `golbal_depth_` 是最大的 `depth_` （被自己蠢到背过头去），它们实际上是决定使用键的哈希值的倒数几位来确定装入哪个容器
- `dir_` 是一个指针数组，而且 `dir_[i]` 和 `dir_[j]` 可能指向同一个容器（我一开始以为必须是两个），如何设计和  `depth_` 与 `golbal_depth_`  密切相关
- 题目给了 `RedistributeBucket` 函数让我们自己实现，这个函数可能会给人误解，我当时看到这个函数，以为每次容器装满时，调整当前容器 `dir_[i]` 就可以了，实际上是需要全局调整 `dir_` 的，因此自己最后并没有实现这个函数，自己在 `Insert` 函数里完成了全局调整

### Task 2 - LRU-K Replacement Policy

这个部分要求实现LRU算法（力扣相关题：[146. LRU 缓存](https://leetcode.cn/problems/lru-cache/)）的改进，即LRU-K算法，LRU-K算法的原理还是比较简单易懂的，但如果没有了解实现的思路，光自己想是很难实现LRU-K的代码的，这里推荐一位大佬的文章：[缓存替换策略：LRU-K算法详解及其C++实现 CMU15-445 Project#1](https://blog.csdn.net/AntiO2/article/details/128439155)，TA甚至直接把论文原文找出来了[The LRU-K page replacement algorithm for database disk buffering (acm.org)](https://dl.acm.org/doi/epdf/10.1145/170036.170081)，真的感谢

代码实现前需要重点理解 `K-distance` 和 `HIST(p)` 的含义（其实还有`LAST(p)`，这个涉及到关联访问，可能优化阶段我会加上，但仅仅完成Task2还不需要用到它），这部分感觉上面这位大佬写得听清楚的，如果英语阅读困难（说的就是我），可以参考；同时还需要理解论文里给出来的伪代码（因为把关联访问和`LAST(p)`的部分删了，所以实际上我实现的伪代码是经过简化的）

尽管伪代码和变量解读分别参考论文和大佬的文章，但伪代码到可运行代码，自己还是一行一行敲出来的（不过目前性能还很差，优化阶段打算引入堆）

### Task 3 - Buffer Pool Manager Instance

Task3应该是整个Project1在思路上最简单的部分了，因为头文件的注释部分给每个函数都讲清楚了实现思路，不同于前面两个Task，Task3的实现思路是能根据注释直接把代码一行一行写出来，但只是这样可能做完对整个 Buffer Pool 的认知还不够，可能还得看下其他大佬的文章

这里只记录下我踩过的两个大坑，后两个坑的解决让我从67走到了100

- `Page.h` 头文件要看下，另外 `BufferPoolManagerInstance` 类作为友元类，是可以直接访问 `Page` 类的私有变量的，因此  `Page` 类不需要提供修改它私有变量的接口
- 每次 `NewPgImp` 和 `FetchPgImp` 操作都会导致内存块（页框，`frame`）对应的 `Page` 的 `pin_count_` 增加，其中  `FetchPgImp`  操作并不是只有在替换内存块的时候才增加
- `UnpinPgImp `操作，只有在当前 `Page` 的`is_dirty_` 为 `false` 的时候才能直接进行更改

### 待优化的部分

目前的性能是很差的，差到我不敢把 `Leaderboard` 上的排名放出来，但目前的打算还是先尽快通关CMU15-445，所以优化得等完成后了，先留个坑。现在能想到的比较明显的可优化部分有两个（当然实际上肯定不止），等回来变优化边记录

- LRU-K部分，每次找 `K-distance` 最大的驱逐块是采用遍历的方式，需要引入堆来优化，
- 全程使用大锁 `std::scoped_lock<std::mutex> lock(latch_)` 来解决多线程问题，可以考虑使用更细致的锁

别的想起来继续补充，这里放一张线上测试满分的图和 `Leaderboard` 排名，目前排名成绩很差，后面好好改进吧，并期待接下来的 Project 2，就网上的信息看，是难度拉满的B+树，希望自己脑细胞还够用吧

<img src="mySources\scores\Project 1 Score.jpg" style="zoom:67%;" />

<img src="mySources\scores\Project 1 Rank 1.jpg" style="zoom:80%;" />

---

## Project 2 - B+Tree

后续补充（断断续续做了有八九天，就难度上来说配得上“噩梦”两字了）

### Task 1 - B+Tree Pages

### Task 2 - B+Tree Data Structure

### Task 3 - Index Iterator

### Task 4 - Concurrent Index

### 待优化的部分

乐观锁和悲观锁（后续补充）

线上测试满分结果及 `Leaderboard` 排名（能比Project 1进步这么多也挺离谱的）

<img src="mySources\scores\Project 2 Checkpoint 1 Score.jpg" style="zoom:67%;" />

<img src="mySources\scores\Project 2 Checkpoint 2 Score.jpg" style="zoom:67%;" />

<img src="mySources\scores\Project 2 Rank 1.jpg" style="zoom:80%;" />

---

## Project 3 - Query Execution

后续补充（难度比Project 2低很多，但读源码的过程能感觉到能深挖很多东西，准备等过段时间没那么忙的时候好好做下 Leaderboard Task，并优化一些比较明显的可以改进的地方，先留个坑）

### Task  1 - Access Method Executors

###Task 2 - Aggregation & Join Executors

### Task 3 - Sort + Limit Executors and Top-N Optimization

### Leaderboard Task（待完成）

### 待优化的部分

还没做的 Leaderboard Task先不管，还有：

使用 simple nested loop join algorithm 的 NestedLoopJoin算子，可以优化成使用更快的 hash join algorithm 的 HashJoin算子（涉及优化器，且Leaderboard Task的第一个任务也和这个有关）

Top-N算子目前是全部排序然后选前n个，但这么做速度较慢，可以引入堆来改进

线上测试满分结果和 `Leaderboard` 现状

<img src="mySources\scores\Project 3 Score.jpg" style="zoom: 67%;" />

<img src="mySources\scores\Project 3 Rank 1.jpg" style="zoom:80%;" />
